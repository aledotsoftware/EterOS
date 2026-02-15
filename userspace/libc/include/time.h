#ifndef _TIME_H
#define _TIME_H

#include <stdint.h>

typedef int64_t time_t;

struct timespec {
    time_t tv_sec;
    long   tv_nsec;
};

/* Clock IDs */
#define CLOCK_REALTIME           0
#define CLOCK_MONOTONIC          1
#define CLOCK_PROCESS_CPUTIME_ID 2
#define CLOCK_THREAD_CPUTIME_ID  3

int nanosleep(const struct timespec *req, struct timespec *rem);
int clock_gettime(int clock_id, struct timespec *tp);

#endif /* _TIME_H */
