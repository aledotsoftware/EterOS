/**
 * éterOS - Fast Userspace Mutex (Futex) Implementation
 * Copyright (c) 2026 Tudex Networks. All rights reserved.
 */

#include <futex.h>
#include <task.h>
#include <mm.h>
#include <lock.h>
#include <errno.h>
#include <string.h>
#include <hal.h>
#include <timer.h>
#include <time.h>

#define FUTEX_BUCKETS 16

/* Internal node structure */
typedef struct futex_node {
    task_t *task;
    uint32_t *uaddr;
    struct futex_node *next;
} futex_node_t;

/* Bucket structure */
typedef struct {
    spinlock_t lock;
    futex_node_t *head;
} futex_bucket_t;

static futex_bucket_t buckets[FUTEX_BUCKETS];

static int futex_hash(uint32_t *uaddr) {
    uint64_t addr = (uint64_t)uaddr;
    return (addr >> 2) % FUTEX_BUCKETS;
}

void futex_init(void) {
    for (int i = 0; i < FUTEX_BUCKETS; i++) {
        buckets[i].lock = 0;
        buckets[i].head = NULL;
    }
}

/* Helper to validate user address */
static int validate_uaddr(uint32_t *uaddr) {
    uint64_t addr = (uint64_t)uaddr;
    /* Check alignment */
    if (addr & 3) return 0;
    /* Check for NULL */
    if (addr == 0) return 0;
    /* Basic user space check (assuming kernel is high half or similar,
       but for now just check non-null and alignment) */
    return 1;
}

int futex_wait(uint32_t *uaddr, uint32_t val, const void *timeout) {
    if (!validate_uaddr(uaddr)) return -EFAULT;

    /* 1. Atomically check value (optimistic check) */
    /* Note: In a real kernel, we need safe user access here (get_user).
       For now, we assume direct access is possible but unsafe. */
    if (*uaddr != val) return -EAGAIN;

    int bucket_idx = futex_hash(uaddr);
    futex_bucket_t *b = &buckets[bucket_idx];

    /* 2. Allocate node before locking to minimize critical section */
    futex_node_t *node = (futex_node_t*)kmalloc(sizeof(futex_node_t));
    if (!node) return -ENOMEM;

    spin_lock(&b->lock);

    /* 3. Re-check value under lock to avoid race */
    if (*uaddr != val) {
        spin_unlock(&b->lock);
        kfree(node);
        return -EAGAIN;
    }

    task_t *current = task_get_current();

    node->task = current;
    node->uaddr = uaddr;
    node->next = b->head;
    b->head = node;

    /* Calculate timeout if provided */
    if (timeout) {
        const struct timespec *ts = (const struct timespec *)timeout;
        uint64_t ticks = (ts->tv_sec * TIMER_HZ) + (((uint64_t)ts->tv_nsec * TIMER_HZ) / 1000000000);
        if (ticks == 0 && (ts->tv_sec > 0 || ts->tv_nsec > 0)) {
            ticks = 1;
        }
        current->wake_tick = timer_get_ticks() + ticks;
    } else {
        current->wake_tick = 0;
    }

    /* 4. Block task */
    current->state = TASK_BLOCKED;

    spin_unlock(&b->lock);

    /* 5. Yield CPU */
    task_yield();

    /* 6. We are back. Check why. */

    spin_lock(&b->lock);

    /* Search for our node */
    futex_node_t **pp = &b->head;
    futex_node_t *curr = b->head;
    int found = 0;

    while (curr) {
        if (curr == node) {
            /* Still in queue! This means spurious wakeup, signal, or timeout. */
            found = 1;
            /* Remove from list */
            *pp = curr->next;
            break;
        }
        pp = &curr->next;
        curr = curr->next;
    }

    spin_unlock(&b->lock);

    /* Free the node that we allocated */
    kfree(node);

    if (found) {
        /* Check if it was a timeout */
        if (current->wake_tick > 0 && timer_get_ticks() >= current->wake_tick) {
            /* Clear wake_tick */
            current->wake_tick = 0;
            return -ETIMEDOUT;
        }

        /* If we were found in the queue but not timed out, return EINTR (spurious/signal) */
        if (current->wake_tick > 0) current->wake_tick = 0;
        return -EINTR;
    }

    /* Woken by futex_wake */
    if (current->wake_tick > 0) current->wake_tick = 0;
    return 0;
}

int futex_wake(uint32_t *uaddr, int count) {
    if (!validate_uaddr(uaddr)) return -EFAULT;

    int bucket_idx = futex_hash(uaddr);
    futex_bucket_t *b = &buckets[bucket_idx];

    spin_lock(&b->lock);

    futex_node_t **pp = &b->head;
    futex_node_t *curr = b->head;
    int woken = 0;

    while (curr && woken < count) {
        if (curr->uaddr == uaddr) {
            /* Wake up this task */
            if (curr->task->state == TASK_BLOCKED) {
                curr->task->state = TASK_READY;
            }

            /* Remove from list */
            futex_node_t *to_remove = curr; /* Kept for logic, but we don't free it */
            (void)to_remove;

            *pp = curr->next;
            curr = curr->next;

            /* We do NOT free the node here. The waiter does it. */
            woken++;
        } else {
            pp = &curr->next;
            curr = curr->next;
        }
    }

    spin_unlock(&b->lock);

    return woken;
}
