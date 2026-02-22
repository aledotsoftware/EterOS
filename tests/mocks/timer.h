#ifndef MOCK_TIMER_H
#define MOCK_TIMER_H

#include <stdint.h>

#define TIMER_HZ 1000

uint64_t timer_get_ticks(void);

#endif
