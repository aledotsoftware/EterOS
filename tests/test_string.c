#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>

/* Define host test mode */
#define __ETEROS_HOST_TEST__
#include "../include/string.h"

int main() {
    printf("Running string tests...\n");

    /* Test itoa_s */
    {
        char buffer[32];
        itoa_s(12345, buffer, sizeof(buffer), 10);
        assert(strcmp(buffer, "12345") == 0);

        itoa_s(-12345, buffer, sizeof(buffer), 10);
        assert(strcmp(buffer, "-12345") == 0);

        itoa_s(0, buffer, sizeof(buffer), 10);
        assert(strcmp(buffer, "0") == 0);

        /* Test truncation */
        char small_buf[4]; /* Holds 3 chars + null */
        itoa_s(12345, small_buf, sizeof(small_buf), 10);
        /* Expect "123" because itoa_s writes most significant digits first */
        printf("Small buffer itoa: %s\n", small_buf);
        assert(strcmp(small_buf, "123") == 0);
    }

    /* Test utoa_hex_s */
    {
        char buffer[32];
        utoa_hex_s(0xDEADBEEF, buffer, sizeof(buffer));
        /* Should be "0x00000000DEADBEEF" */
        printf("Full buffer utoa_hex: %s\n", buffer);
        assert(strcmp(buffer, "0x00000000DEADBEEF") == 0);

        /* Test truncation */
        char small_buf[5]; /* Holds 4 chars + null */
        utoa_hex_s(0xDEADBEEF, small_buf, sizeof(small_buf));
        /* Should contain "0x00" */
        printf("Small buffer utoa_hex: %s\n", small_buf);
        assert(strcmp(small_buf, "0x00") == 0);
    }

    /* Test memset16 */
    {
        uint16_t buffer[16];
        /* Fill with pattern 0xAABB */
        memset16(buffer, 0xAABB, 16);
        for(int i=0; i<16; i++) {
            assert(buffer[i] == 0xAABB);
        }
        printf("memset16 test passed\n");
    }

    /* Test memmove (overlap handling) */
    /* Note: Since we link with kernel/string.c, we are testing our implementation
       if the linker picks it up. If not, we test libc's.
       To be sure, let's trust that memset16 is unique.
       Testing memmove/memcpy here might be ambiguous due to libc conflict.
    */

    printf("All tests passed!\n");
    return 0;
}
