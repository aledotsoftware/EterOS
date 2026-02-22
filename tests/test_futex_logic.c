#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <time.h> /* For struct timespec */

/* --- Mocks --- */
#include "mocks/futex.h"
#include "mocks/task.h"
#include "mocks/mm.h"
#include "mocks/lock.h"
#include "mocks/errno.h"
#include "mocks/timer.h"

/* Global state for test */
task_t task1 = {1, TASK_RUNNING, 0, NULL};
task_t task2 = {2, TASK_RUNNING, 0, NULL};
task_t *current_task = &task1;

uint64_t current_ticks = 0;
uint64_t ticks_to_add_on_yield = 0;

/* Mock Implementations */

void spin_lock(spinlock_t *lock) {
    *lock = 1;
}

void spin_unlock(spinlock_t *lock) {
    *lock = 0;
}

void *kmalloc(size_t size) {
    return malloc(size);
}

void kfree(void *ptr) {
    free(ptr);
}

task_t *task_get_current(void) {
    return current_task;
}

void task_yield(void) {
    current_ticks += ticks_to_add_on_yield;
}

uint64_t timer_get_ticks(void) {
    return current_ticks;
}

/* Include the source file under test */
#include "../kernel/futex.c"

/* --- Tests --- */

void test_futex_wait_mismatch(void) {
    printf("Test: futex_wait mismatch...\n");
    uint32_t uaddr = 100;
    int ret = futex_wait(&uaddr, 101, NULL); /* Expected 101, but is 100 */
    assert(ret == -EAGAIN);
    printf("PASS\n");
}

void test_futex_wait_success(void) {
    printf("Test: futex_wait success...\n");

    uint32_t uaddr = 200;
    ticks_to_add_on_yield = 0;

    /* Simulate wait. Since task_yield returns immediately,
       we check if we are still in queue (EINTR). */

    int ret = futex_wait(&uaddr, 200, NULL);
    assert(ret == -EINTR); /* Because not woken */
    assert(task1.state == TASK_BLOCKED);

    /* Clean up: manually remove from bucket to avoid mem leak in test */
    int idx = futex_hash(&uaddr);
    futex_bucket_t *b = &buckets[idx];
    b->head = NULL;

    printf("PASS\n");
}

void test_futex_wake_logic(void) {
    printf("Test: futex_wake logic...\n");

    uint32_t uaddr = 300;

    /* Manually insert node */
    int idx = futex_hash(&uaddr);

    futex_node_t *node = malloc(sizeof(futex_node_t));
    node->task = &task2;
    node->uaddr = &uaddr;
    node->next = NULL;

    buckets[idx].head = node;
    task2.state = TASK_BLOCKED;

    /* Wake */
    int count = futex_wake(&uaddr, 1);

    assert(count == 1);
    assert(task2.state == TASK_READY);
    assert(buckets[idx].head == NULL);

    free(node); /* In real code, waiter frees it. Here we do it. */
    printf("PASS\n");
}

void test_futex_wait_timeout(void) {
    printf("Test: futex_wait timeout...\n");

    uint32_t uaddr = 400;
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 100 * 1000 * 1000; /* 100ms */

    /*
       TIMER_HZ = 1000
       100ms = 100 ticks

       We start at current_ticks = 0.
       futex_wait calculates timeout ticks = 100.
       wake_tick = 100.

       We set ticks_to_add_on_yield = 150.
       So after yield, current_ticks = 150.

       When futex_wait checks timeout: 150 >= 100 -> ETIMEDOUT.
    */

    current_ticks = 0;
    ticks_to_add_on_yield = 150;

    int ret = futex_wait(&uaddr, 400, &ts);

    assert(ret == -ETIMEDOUT);

    /* Verify we are removed from bucket */
    int idx = futex_hash(&uaddr);
    assert(buckets[idx].head == NULL);

    printf("PASS\n");
}

int main(void) {
    futex_init();

    test_futex_wait_mismatch();
    test_futex_wait_success();
    test_futex_wake_logic();
    test_futex_wait_timeout();

    printf("All tests passed.\n");
    return 0;
}
