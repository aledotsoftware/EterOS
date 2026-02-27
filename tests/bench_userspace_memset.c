#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <string.h>

// Rename to avoid conflict
#define memset my_memset
#define memcpy my_memcpy

// Stub helpers not present in file
void *memmove(void *dest, const void *src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
size_t strlen(const char *s);
size_t strnlen(const char *s, size_t maxlen);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
char *strncpy(char *dest, const char *src, size_t n);
size_t strlcpy(char *dest, const char *src, size_t size);
size_t strlcat(char *dest, const char *src, size_t size);
char *strchr(const char *s, int c);
char *strrchr(const char *s, int c);
char *strstr(const char *haystack, const char *needle);

// Include userspace implementation
#include "../userspace/libc/src/string.c"

// Provide stubs for linker if needed (but we are including .c so functions are static or defined here)
// Actually we included the .c file so we have definitions.
// But the .c file has other functions too. We only need memset/memcpy for this bench.

void benchmark_memset() {
    const size_t size = 100 * 1024 * 1024; // 100 MB
    const size_t iterations = 50;

    char* buffer = malloc(size);
    if (!buffer) return;

    printf("Benchmarking userspace memset (C loop 8-byte)...\n");
    clock_t start = clock();

    for (size_t i = 0; i < iterations; i++) {
        my_memset(buffer, 0xAB, size);
        // Prevent opt
        __asm__ volatile("" : : "r"(buffer) : "memory");
    }

    clock_t end = clock();
    double time_taken = ((double)(end - start)) / CLOCKS_PER_SEC;

    printf("Time: %f s\n", time_taken);
    printf("Bandwidth: %f GB/s\n", (double)(size * iterations) / time_taken / (1024.0*1024.0*1024.0));

    free(buffer);
}

int main() {
    benchmark_memset();
    return 0;
}
