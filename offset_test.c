#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    CPU_STATE_OFFLINE = 0,
    CPU_STATE_BOOTING,
    CPU_STATE_ONLINE,
    CPU_STATE_HALTED
} cpu_state_t;

typedef struct {
    uint64_t self;
    uint32_t apic_id;
    uint32_t acpi_id;
    uint32_t index;
    volatile cpu_state_t state;
    void* current_task;
    void* idle_task;
    uint32_t sched_ticks;
    void* gdt;
    void* tss;
    uint64_t kernel_stack_top;
    uint64_t user_stack_scratch;
    uint64_t context_switches;
    uint64_t uptime_ticks;
} __attribute__((aligned(64))) cpu_info_t;

int main() {
    printf("kernel_stack_top offset: %zu\n", offsetof(cpu_info_t, kernel_stack_top));
    printf("user_stack_scratch offset: %zu\n", offsetof(cpu_info_t, user_stack_scratch));
    return 0;
}
