#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

// Mocking the environment for the test
#ifndef __ETEROS_HOST_TEST__
#define __ETEROS_HOST_TEST__
#endif

// Include source directly to test implementation
// Rename to avoid conflict with host libc
#define memset my_memset
#define memcpy my_memcpy

// Stub helpers not present in file but needed if string.c calls them (it doesn't for memset)
size_t strlen(const char *s); // Stub
// ...

// Include the userspace implementation
#include "../userspace/libc/src/string.c"

#undef memset
// Now 'my_memset' is the function from string.c

void benchmark_memset() {
    const size_t size = 100 * 1024 * 1024; // 100 MB
    const size_t iterations = 100;

    char* buffer = malloc(size);
    if (!buffer) {
        printf("Failed to allocate buffer\n");
        return;
    }

    printf("Benchmarking userspace memset...\n");
    printf("Block size: %zu MB\n", size / (1024 * 1024));

    clock_t start = clock();

    for (size_t i = 0; i < iterations; i++) {
        my_memset(buffer, 0xAB, size);
        __asm__ volatile("" : : "r"(buffer) : "memory");
    }

    clock_t end = clock();
    double time_taken = ((double)(end - start)) / CLOCKS_PER_SEC;

    printf("Time taken: %f seconds\n", time_taken);
    printf("Bandwidth: %f GB/s\n", (double)(size * iterations) / time_taken / (1024.0*1024.0*1024.0));

    free(buffer);
}

int main() {
    benchmark_memset();
    return 0;
}
