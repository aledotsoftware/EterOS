#ifndef __ETEROS_HOST_TEST__
#define __ETEROS_HOST_TEST__
#endif

#define ETEROS_LOCK_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>

/* Mock types */
#include "../include/types.h"

/* Mock memory management */
void* kmalloc(size_t size) {
    return malloc(size);
}

void kfree(void* ptr) {
    free(ptr);
}

/* Mock spinlocks */
typedef volatile int spinlock_t;
void spin_lock(spinlock_t *lock) { (void)lock; }
void spin_unlock(spinlock_t *lock) { (void)lock; }
int spin_trylock(spinlock_t *lock) { (void)lock; return 1; }

/* Include source under test */
#include "../kernel/fs/bcache.c"

void test_bcache_invalidate_all() {
    printf("Running test_bcache_invalidate_all...\n");

    bcache_init();

    uint8_t write_data1[512];
    uint8_t write_data2[512];
    memset(write_data1, 0xAA, 512);
    memset(write_data2, 0xBB, 512);

    /* Fill the cache */
    bcache_write(1, 10, write_data1);
    bcache_write(1, 20, write_data2);

    uint8_t read_buffer[512];

    /* Verify it's in the cache */
    int res = bcache_read(1, 10, read_buffer);
    if (res != 0) {
        printf("FAILED: bcache_read(1, 10) missed after write\n");
        exit(1);
    }
    if (memcmp(read_buffer, write_data1, 512) != 0) {
        printf("FAILED: bcache_read(1, 10) data mismatch\n");
        exit(1);
    }

    res = bcache_read(1, 20, read_buffer);
    if (res != 0) {
        printf("FAILED: bcache_read(1, 20) missed after write\n");
        exit(1);
    }
    if (memcmp(read_buffer, write_data2, 512) != 0) {
        printf("FAILED: bcache_read(1, 20) data mismatch\n");
        exit(1);
    }

    /* Invalidate all */
    bcache_invalidate_all();

    /* Verify the cache is empty */
    res = bcache_read(1, 10, read_buffer);
    if (res != -1) {
        printf("FAILED: bcache_read(1, 10) hit after invalidate_all\n");
        exit(1);
    }

    res = bcache_read(1, 20, read_buffer);
    if (res != -1) {
        printf("FAILED: bcache_read(1, 20) hit after invalidate_all\n");
        exit(1);
    }

    printf("PASSED test_bcache_invalidate_all\n");
}

int main() {
    test_bcache_invalidate_all();
    printf("All bcache tests passed!\n");
    return 0;
}
