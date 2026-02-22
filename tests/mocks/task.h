#ifndef MOCK_TASK_H
#define MOCK_TASK_H

#include <stdint.h>

typedef enum {
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_SLEEPING,
    TASK_DEAD
} task_state_t;

typedef struct task {
    uint32_t id;
    task_state_t state;
    uint64_t wake_tick;
    struct task *next;
} task_t;

task_t* task_get_current(void);
void task_yield(void);

#endif
