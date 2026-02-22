#ifndef MOCK_FUTEX_H
#define MOCK_FUTEX_H

#include <stdint.h>

void futex_init(void);
int futex_wait(uint32_t *uaddr, uint32_t val, const void *timeout);
int futex_wake(uint32_t *uaddr, int count);

#endif
