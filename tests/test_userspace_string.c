#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h> // Host string.h
#include <assert.h>

// Macros
#define memcpy eteros_memcpy
#define memset eteros_memset
#define memmove eteros_memmove
#define memcmp eteros_memcmp
#define strlen eteros_strlen
#define strcmp eteros_strcmp
#define strncmp eteros_strncmp
#define strncpy eteros_strncpy
#define strlcpy eteros_strlcpy
#define strlcat eteros_strlcat
#define strchr eteros_strchr
#define strrchr eteros_strrchr
#define strstr eteros_strstr
#define strtok eteros_strtok

// Include the implementation directly to test static/internal functions if any,
// and to avoid linking issues.
#include "../userspace/libc/src/string.c"

void test_strlen() {
    assert(strlen("") == 0);
    assert(strlen("a") == 1);
    assert(strlen("ab") == 2);
    assert(strlen("abc") == 3);
    assert(strlen("1234567") == 7);
    assert(strlen("12345678") == 8);
    assert(strlen("123456789") == 9);

    // Alignment tests
    char *buf = malloc(256);
    if (!buf) return;

    // Fill with 'A'
    for (int i=0; i<256; i++) buf[i] = 'A';

    // Test various start offsets (0 to 15 to cover alignment)
    for (int offset = 0; offset < 16; offset++) {
        // Test various lengths
        for (int len = 0; len < 128; len++) {
            buf[offset + len] = '\0';
            assert(strlen(buf + offset) == (size_t)len);
            buf[offset + len] = 'A'; // Restore
        }
    }
    free(buf);

    printf("strlen correctness tests passed!\n");
}

void test_strchr() {
    const char* str = "Hello World";

    /* Found at start */
    assert(strchr(str, 'H') == str);

    /* Found in middle */
    assert(strchr(str, 'o') == str + 4);

    /* Found at end (last char) */
    assert(strchr(str, 'd') == str + 10);

    /* Found null terminator */
    assert(strchr(str, '\0') == str + 11);

    /* Not found */
    assert(strchr(str, 'Z') == 0);

    /* Empty string */
    const char* empty = "";
    assert(strchr(empty, '\0') == empty);
    assert(strchr(empty, 'A') == 0);

    /* Alignment tests */
    /* Use a buffer with known alignment */
    char* buf = malloc(256);
    if (buf) {
        memset(buf, 'X', 255);
        buf[255] = '\0';

        /* Place target at various positions */
        for (int i = 0; i < 255; i++) {
            buf[i] = 'Y';

            // Test searching for 'Y' at position i
            // We iterate through various start pointers (alignments)
            // But strchr takes a start pointer.

            // Just verify searching from start works
            assert(strchr(buf, 'Y') == buf + i);

            buf[i] = 'X'; /* Restore */
        }

        // Advanced alignment test: search inside aligned/unaligned blocks
        for (int align = 0; align < 16; align++) {
            char* ptr = buf + align;
            // Create a string at 'ptr'
            // Length e.g. 64
            int len = 64;
            if (align + len >= 256) continue;

            ptr[len] = '\0'; // Terminate

            // Test not found
            assert(strchr(ptr, 'Z') == 0);

            // Test found at various positions relative to ptr
            for (int pos = 0; pos < len; pos++) {
                ptr[pos] = 'Z';
                assert(strchr(ptr, 'Z') == ptr + pos);
                ptr[pos] = 'X'; // Restore
            }

            // Restore terminator
            ptr[len] = 'X';
        }

        free(buf);
    }

    printf("strchr correctness tests passed!\n");
}

int main() {
    test_strlen();
    test_strchr();
    return 0;
}
