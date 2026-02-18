#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

/* Capture standard memmove */
static void* (*std_memmove)(void*, const void*, size_t) = memmove;

#ifndef __ETEROS_HOST_TEST__
#define __ETEROS_HOST_TEST__
#endif

#include "../include/string.h"

#define BUFFER_SIZE (16 * 1024 * 1024) // 16 MB
#define COPY_SIZE   (8 * 1024 * 1024)  // 8 MB
#define ITERATIONS  1000

void benchmark_memmove_backward() {
    printf("Benchmarking eteros_memmove (BACKWARD COPY dest > src)...\n");

    char* buffer = malloc(BUFFER_SIZE);
    if (!buffer) {
        printf("Malloc failed\n");
        return;
    }

    // Initialize buffer
    memset(buffer, 0xAA, BUFFER_SIZE);

    // Backward copy: dest = buffer + OFFSET, src = buffer
    // Overlap: src range [0, COPY_SIZE)
    //          dest range [1, COPY_SIZE + 1)
    // Since dest > src, this triggers the backward copy path.

    char* src = buffer;
    char* dest = buffer + 1; // 1 byte offset to force unalignment issues if any, and overlap

    clock_t start = clock();

    volatile int dummy = 0;
    for (int i = 0; i < ITERATIONS; i++) {
        memmove(dest, src, COPY_SIZE);
        dummy++; // Prevent overly aggressive compiler optimizations on the loop
    }

    clock_t end = clock();
    double time_taken = ((double)(end - start)) / CLOCKS_PER_SEC;

    printf("  Time: %f seconds (%d iterations of %d MB)\n", time_taken, ITERATIONS, COPY_SIZE / (1024*1024));
    printf("  Throughput: %f GB/s\n", (double)(ITERATIONS * (long long)COPY_SIZE) / time_taken / (1024.0*1024.0*1024.0));

    free(buffer);
}

void benchmark_memmove_forward() {
    printf("Benchmarking eteros_memmove (FORWARD COPY dest < src)...\n");

    char* buffer = malloc(BUFFER_SIZE);
    if (!buffer) {
        printf("Malloc failed\n");
        return;
    }

    memset(buffer, 0xAA, BUFFER_SIZE);

    // Forward copy: dest < src
    char* dest = buffer;
    char* src = buffer + 1;

    clock_t start = clock();

    volatile int dummy = 0;
    for (int i = 0; i < ITERATIONS; i++) {
        memmove(dest, src, COPY_SIZE);
        dummy++;
    }

    clock_t end = clock();
    double time_taken = ((double)(end - start)) / CLOCKS_PER_SEC;

    printf("  Time: %f seconds\n", time_taken);
    printf("  Throughput: %f GB/s\n", (double)(ITERATIONS * (long long)COPY_SIZE) / time_taken / (1024.0*1024.0*1024.0));

    free(buffer);
}

int main() {
    benchmark_memmove_backward();
    benchmark_memmove_forward();
    return 0;
}
