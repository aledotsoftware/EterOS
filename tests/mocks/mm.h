#ifndef MOCK_MM_H
#define MOCK_MM_H

#include <stddef.h>

void *kmalloc(size_t size);
void kfree(void *ptr);

#endif
