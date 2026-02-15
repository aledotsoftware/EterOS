/**
 * éterOS - Task Scheduler (Multicore Ready)
 * Copyright (c) 2026 Tudex Networks. All rights reserved.
 */

#include "../include/task.h"
#include "../include/mm.h"
#include "../include/string.h"
#include "../include/serial.h"
#include "../include/vga.h"
#include "../include/timer.h"
#include "../include/gdt.h"
#include "../include/cpu.h"
#include "../include/msr.h"
#include "../include/lock.h"

/* ========================================================================= */
/* Estado Global del Scheduler                                               */
/* ========================================================================= */

static task_t   tasks[MAX_TASKS] __attribute__((aligned(16)));
static int      task_count    = 0;
static uint32_t next_id       = 0;
static bool     scheduler_active = false;
static spinlock_t sched_lock = 0;

/* Global load metrics (aggregate or BSP) */
static int global_cpu_load = 0;

/* ========================================================================= */
/* Task Entry Wrapper                                                        */
/* ========================================================================= */

typedef void (*task_func_t)(void);

static void task_entry_wrapper(void) {
    __asm__ volatile("sti");

    task_t* self = task_get_current();
    if (self && self->entry) {
        self->entry();
    }

    task_exit();
}

/* ========================================================================= */
/* API del Scheduler                                                         */
/* ========================================================================= */

void scheduler_init(void) {
    spin_lock(&sched_lock);
    memset(tasks, 0, sizeof(tasks));

    cpu_info_t* bsp = &cpus[0];

    /* Tarea 0: Kernel/Shell */
    tasks[0].id = next_id++;
    tasks[0].state = TASK_RUNNING;
    tasks[0].stack_base = NULL;
    tasks[0].rsp = 0;

    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    tasks[0].cr3 = cr3;
    tasks[0].kernel_stack = 0x90000; /* BSP Boot Stack */

    strlcpy(tasks[0].name, "kernel", sizeof(tasks[0].name));

    task_count = 1;
    bsp->current_task = &tasks[0];
    bsp->sched_ticks = 0;
    
    scheduler_active = true;
    spin_unlock(&sched_lock);

    serial_write_string("[SCHED] Multicore Scheduler initialized (BSP Ready)\n");
}

void task_init_ap(void) {
    cpu_info_t* cpu = get_current_cpu();
    serial_write_string("[SCHED] Initializing scheduler for AP ");
    
    /* Create an idle task for this AP */
    char idle_name[16];
    strlcpy(idle_name, "idle", sizeof(idle_name));
    /* TODO: append CPU index to name */
    
    /* The AP entry point will eventually become the idle loop */
    /* For now, we just mark it as ONLINE and let it halt in smp.c */
}

int task_create(const char* name, void (*entry)(void)) {
    spin_lock(&sched_lock);
    
    if (task_count >= MAX_TASKS) {
        spin_unlock(&sched_lock);
        return -1;
    }

    int slot = -1;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_DEAD || (i >= task_count && tasks[i].state == 0)) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) {
        spin_unlock(&sched_lock);
        return -1;
    }

    uint8_t* stack = (uint8_t*)kmalloc(TASK_STACK_SIZE);
    if (!stack) {
        spin_unlock(&sched_lock);
        return -1;
    }
    memset(stack, 0, TASK_STACK_SIZE);

    tasks[slot].id = next_id++;
    tasks[slot].state = TASK_READY;
    tasks[slot].stack_base = stack;
    tasks[slot].kernel_stack = (uint64_t)(stack + TASK_STACK_SIZE);

    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    tasks[slot].cr3 = cr3;

    strlcpy(tasks[slot].name, name, sizeof(tasks[slot].name));
    tasks[slot].entry = entry;

    uint64_t* sp = (uint64_t*)(stack + TASK_STACK_SIZE);
    *(--sp) = (uint64_t)task_entry_wrapper;
    *(--sp) = 0; /* rbp */
    *(--sp) = 0; /* rbx */
    *(--sp) = 0; /* r12 */
    *(--sp) = 0; /* r13 */
    *(--sp) = 0; /* r14 */
    *(--sp) = (uint64_t)entry; /* r15 */

    tasks[slot].rsp = (uint64_t)sp;

    if (slot >= task_count) {
        task_count = slot + 1;
    }

    spin_unlock(&sched_lock);
    serial_write_string("[SCHED] Task created: ");
    serial_write_string(name);
    serial_write_string("\n");

    return (int)tasks[slot].id;
}

static int find_next_task(int current_idx) {
    int next = current_idx;
    for (int i = 0; i < task_count; i++) {
        next = (next + 1) % task_count;
        /* Simple load balancing: check if task is READY */
        if (tasks[next].state == TASK_READY) {
            return next;
        }
    }
    
    /* If current task is still runnable, keep it */
    if (tasks[current_idx].state == TASK_RUNNING) {
        return current_idx;
    }

    return -1;
}

void schedule(void) {
    if (!scheduler_active) return;

    __asm__ volatile("cli");
    cpu_info_t* cpu = get_current_cpu();
    if (!cpu) return;

    spin_lock(&sched_lock);

    task_t* current = (task_t*)cpu->current_task;
    int current_idx = -1;
    
    /* Find index of current task in global array (inefficient but works for now) */
    if (current) {
        for (int i = 0; i < task_count; i++) {
            if (&tasks[i] == current) {
                current_idx = i;
                break;
            }
        }
    }

    /* Check if we should switch */
    if (current && current->state == TASK_RUNNING) {
        cpu->sched_ticks++;
        if (cpu->sched_ticks < SCHEDULER_HZ) {
            spin_unlock(&sched_lock);
            __asm__ volatile("sti");
            return;
        }
    }
    cpu->sched_ticks = 0;

    int next_idx = find_next_task(current_idx == -1 ? 0 : current_idx);

    if (next_idx == -1 || (&tasks[next_idx] == current && current->state == TASK_RUNNING)) {
        spin_unlock(&sched_lock);
        __asm__ volatile("sti");
        return;
    }

    task_t* old_task = current;
    task_t* new_task = &tasks[next_idx];

    if (old_task && old_task->state == TASK_RUNNING) {
        old_task->state = TASK_READY;
    }

    new_task->state = TASK_RUNNING;
    cpu->current_task = new_task;

    /* Update TSS for this CPU */
    if (new_task->kernel_stack != 0) {
        tss_set_rsp0(new_task->kernel_stack);
        cpu->kernel_stack_top = new_task->kernel_stack;
    }

    /* Context Switch */
    /* Note: context_switch doesn't need the lock, but we must protect old_task->rsp write? 
       Actually, only THIS CPU will ever write to old_task->rsp while it's its current task. */
    
    uint64_t* old_rsp_ptr = old_task ? &old_task->rsp : NULL;
    uint64_t new_rsp = new_task->rsp;

    spin_unlock(&sched_lock);
    
    if (old_rsp_ptr) {
        context_switch(old_rsp_ptr, new_rsp);
    } else {
        /* This should only happen during first ever switch on a new CPU */
        /* We need a variant of context_switch or just a jump */
        __asm__ volatile("mov %0, %%rsp; ret" : : "r"(new_rsp));
    }

    __asm__ volatile("sti");
}

task_t* task_get_current(void) {
    cpu_info_t* cpu = get_current_cpu();
    return cpu ? (task_t*)cpu->current_task : NULL;
}

void task_yield(void) {
    cpu_info_t* cpu = get_current_cpu();
    if (cpu) cpu->sched_ticks = SCHEDULER_HZ;
    schedule();
}

void task_sleep(uint64_t ms) {
    if (!scheduler_active) {
        timer_wait((uint32_t)ms);
        return;
    }

    uint64_t ticks = ((uint64_t)ms * TIMER_HZ) / 1000;
    if (ms > 0 && ticks == 0) ticks = 1;

    task_t* current = task_get_current();
    if (current) {
        current->wake_tick = timer_get_ticks() + ticks;
        current->state = TASK_SLEEPING;
        task_yield();
        
        while (current->state == TASK_SLEEPING) {
            __asm__ volatile("hlt");
        }
    }
}

void task_wake_expired(uint64_t current_tick) {
    if (!scheduler_active) return;
    
    /* We could lock here, but checking wake_tick is mostly safe? 
       No, better lock. */
    if (spin_trylock(&sched_lock)) {
        for (int i = 0; i < task_count; i++) {
            if (tasks[i].state == TASK_SLEEPING) {
                if (current_tick >= tasks[i].wake_tick) {
                    tasks[i].state = TASK_READY;
                }
            }
        }
        spin_unlock(&sched_lock);
    }
}

void task_exit(void) {
    __asm__ volatile("cli");
    task_t* current = task_get_current();
    
    spin_lock(&sched_lock);
    if (current) {
        current->state = TASK_DEAD;
    }
    spin_unlock(&sched_lock);
    
    schedule();
    
    for (;;) { __asm__ volatile("hlt"); }
}

int task_get_count(void) {
    int count = 0;
    spin_lock(&sched_lock);
    for (int i = 0; i < task_count; i++) {
        if (tasks[i].state != TASK_DEAD && tasks[i].state != 0) {
            count++;
        }
    }
    spin_unlock(&sched_lock);
    return count;
}

int task_kill(uint32_t pid) {
    spin_lock(&sched_lock);
    for (int i = 1; i < task_count; i++) {
        if (tasks[i].id == pid && tasks[i].state != TASK_DEAD) {
            tasks[i].state = TASK_DEAD;
            spin_unlock(&sched_lock);
            return 0;
        }
    }
    spin_unlock(&sched_lock);
    return -1;
}

task_t* task_get_at(int index) {
    if (index >= 0 && index < MAX_TASKS) {
        return &tasks[index];
    }
    return NULL;
}

int task_get_max(void) {
    return MAX_TASKS;
}

task_t* task_get_by_id(uint32_t id) {
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].id == id && tasks[i].state != TASK_DEAD) {
            return &tasks[i];
        }
    }
    return NULL;
}

int task_get_cpu_load(void) {
    return global_cpu_load;
}
