#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

// Mocking the environment for the test
#ifndef __ETEROS_HOST_TEST__
#define __ETEROS_HOST_TEST__
#endif

// Include source directly to test implementation
// We need to define macro for eteros_memset to work as expected if we are including string.c
// But string.c doesn't use eteros_ prefix unless __ETEROS_HOST_TEST__ is defined AND string.h handles it.
// However, we include string.c directly which defines 'memset'.
// To test it against host memset, we should rename the function in string.c or use a wrapper.
// Since we are including .c, the function name is 'memset'. But that conflicts with <string.h> memset.
//
// The trick used in other tests is likely:
#define memset my_memset
#define memcpy my_memcpy
#define memmove my_memmove
#define memcmp my_memcmp
// ... etc

// Stub helpers not present in file but needed if string.c calls them (it doesn't for memset)
size_t strlen(const char *s); // Stub
// ...

// Include the userspace implementation
#include "../userspace/libc/src/string.c"

#undef memset
// Now 'my_memset' is the function from string.c

// Verify memset implementation with extensive checks
void verify_memset() {
    printf("Verifying userspace memset implementation...\n");
    const int MAX_SIZE = 256;
    uint8_t buffer[MAX_SIZE + 16]; // Extra space for alignment checks
    uint8_t reference[MAX_SIZE + 16];

    // Align buffer to 16 bytes for some baseline
    uint8_t* p = (uint8_t*)(((uintptr_t)buffer + 15) & ~15);
    uint8_t* ref = (uint8_t*)(((uintptr_t)reference + 15) & ~15);

    // Test different sizes
    int sizes[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 15, 16, 17, 31, 32, 33, 63, 64, 65, 127, 128, 129, 255};
    int num_sizes = sizeof(sizes) / sizeof(sizes[0]);

    // Test different values
    int values[] = {0x00, 0xFF, 0x55, 0xAA, 0x12};
    int num_values = sizeof(values) / sizeof(values[0]);

    // Test different alignments (0 to 7)
    for (int align = 0; align < 8; align++) {
        for (int v_idx = 0; v_idx < num_values; v_idx++) {
            int val = values[v_idx];

            for (int s_idx = 0; s_idx < num_sizes; s_idx++) {
                int size = sizes[s_idx];

                // Clear buffers first with known pattern using host memset
                memset(p, 0xCC, MAX_SIZE);
                memset(ref, 0xCC, MAX_SIZE);

                // Apply userspace memset (renamed to my_memset) to aligned offset
                void* res = my_memset(p + align, val, size);

                // Apply host memset as reference
                memset(ref + align, val, size);

                // Check return value
                if (res != p + align) {
                    printf("FAIL: Return value mismatch for size %d, align %d\n", size, align);
                    exit(1);
                }

                // Check content
                if (memcmp(p, ref, MAX_SIZE) != 0) {
                    printf("FAIL: Content mismatch for size %d, align %d, val 0x%02X\n", size, align, val);

                    // Dump memory for debug
                    printf("Expected: ");
                    for(int k=0; k<size+align+2; k++) printf("%02X ", ref[k]);
                    printf("\nActual:   ");
                    for(int k=0; k<size+align+2; k++) printf("%02X ", p[k]);
                    printf("\n");

                    exit(1);
                }
            }
        }
    }
    printf("SUCCESS: All memset verification tests passed.\n");
}

int main() {
    verify_memset();
    return 0;
}
