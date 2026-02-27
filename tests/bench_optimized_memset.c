#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

// Mocking the environment for the test
#define __x86_64__

// We want to test the new implementation, so we will inline it here or
// define it as `optimized_memset` and compare.

void *optimized_memset(void *s, int c, size_t n) {
    uint8_t *p = (uint8_t *)s;
    uint8_t val = (uint8_t)c;

#ifdef __x86_64__
    // Use rep stosq for bulk set
    size_t qwords = n / 8;
    size_t remainder = n % 8;

    // Create 64-bit pattern
    uint64_t pattern = val * 0x0101010101010101ULL;

    __asm__ volatile (
        "cld; rep stosq"
        : "+D"(p), "+c"(qwords)
        : "a"(pattern)
        : "memory"
    );

    __asm__ volatile (
        "rep stosb"
        : "+D"(p), "+c"(remainder)
        : "a"(val)
        : "memory"
    );
    return s;
#else
    // Fallback
    while (n--) *p++ = val;
    return s;
#endif
}


void benchmark_optimized_memset() {
    const size_t size = 100 * 1024 * 1024; // 100 MB
    const size_t iterations = 50;

    char* buffer = malloc(size);
    if (!buffer) return;

    printf("Benchmarking optimized memset (rep stosq)...\n");
    clock_t start = clock();

    for (size_t i = 0; i < iterations; i++) {
        optimized_memset(buffer, 0xAB, size);
        __asm__ volatile("" : : "r"(buffer) : "memory");
    }

    clock_t end = clock();
    double time_taken = ((double)(end - start)) / CLOCKS_PER_SEC;

    printf("Time: %f s\n", time_taken);
    printf("Bandwidth: %f GB/s\n", (double)(size * iterations) / time_taken / (1024.0*1024.0*1024.0));

    free(buffer);
}

int main() {
    benchmark_optimized_memset();
    return 0;
}
