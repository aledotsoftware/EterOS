#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MOCK_WIDTH 1024
#define MOCK_HEIGHT 768

uint32_t* mock_fb;

/* Mocks for Framebuffer - Manual prototypes to avoid header dependency hell */
uint32_t* framebuffer_get_buffer(void) { return mock_fb; }
uint32_t framebuffer_get_pitch(void) { return MOCK_WIDTH * 4; }
uint32_t framebuffer_get_width(void) { return MOCK_WIDTH; }
uint32_t framebuffer_get_height(void) { return MOCK_HEIGHT; }
uint32_t framebuffer_get_bpp(void) { return 32; }
void framebuffer_putpixel(uint32_t x, uint32_t y, uint32_t color) { mock_fb[y * MOCK_WIDTH + x] = color; }
void framebuffer_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    for(int i=0; i<h; i++)
        for(int j=0; j<w; j++)
            framebuffer_putpixel(x+j, y+i, color);
}
void framebuffer_putchar(char c, uint32_t x, uint32_t y, uint32_t fg, uint32_t bg) {}

/* Mocks for MM */
void* kmalloc(size_t size) { return malloc(size); }
void kfree(void* ptr) { free(ptr); }

/* Mocks for Serial */
void serial_write_string(const char* s) { /* printf("%s", s); */ }

/* Mocks for Initrd */
void* initrd_read_file(const char* path, uint32_t* size) { return NULL; }

/* Mocks for UPNG (Opaque) */
typedef void upng_t;
upng_t* upng_new_from_bytes(const unsigned char* buffer, unsigned long size) { return NULL; }
void upng_free(upng_t* upng) {}
void upng_decode(upng_t* upng) {}
unsigned upng_get_error(const upng_t* upng) { return 1; }
unsigned upng_get_width(const upng_t* upng) { return 0; }
unsigned upng_get_height(const upng_t* upng) { return 0; }
unsigned upng_get_format(const upng_t* upng) { return 0; }
const unsigned char* upng_get_buffer(const upng_t* upng) { return NULL; }

/* Mock for Font */
const uint8_t font8x16[256 * 16];

/* Include Omni source to access internal functions if needed, but we link against it */
// #include "../include/ui/omni.h"
/* Manually declare Omni functions to avoid include path issues with time.h */
void omni_init(void);
void omni_begin_frame(void);
void omni_fill_gradient_v(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color_top, uint32_t color_bottom);

/* Benchmark function */
void benchmark_gradient() {
    mock_fb = (uint32_t*)malloc(MOCK_WIDTH * MOCK_HEIGHT * 4);
    if (!mock_fb) {
        printf("Failed to allocate mock framebuffer\n");
        return;
    }

    omni_init();
    omni_begin_frame();

    /* Fill with known pattern */
    memset(mock_fb, 0, MOCK_WIDTH * MOCK_HEIGHT * 4);

    uint32_t start_col = 0xFF0000; /* Red */
    uint32_t end_col = 0x0000FF;   /* Blue */

    clock_t start = clock();
    int iterations = 1000;

    for (int i = 0; i < iterations; i++) {
        /* Draw gradient over full screen or card size */
        /* Use card size to be realistic: 180x140 */
        /* But do it many times per frame? Or just many frames? */
        /* Let's do many calls per frame to simulate load */

        /* 17 cards per frame in gui_demo.c */
        /* So 1000 frames of 17 cards = 17000 calls */

        for (int c = 0; c < 17; c++) {
            omni_fill_gradient_v(100 + (c*10), 100 + (c*10), 180, 140, start_col, end_col);
        }
    }

    clock_t end = clock();
    double time_taken = ((double)(end - start)) / CLOCKS_PER_SEC;

    printf("Benchmark omni_fill_gradient_v: %d iterations (17 cards each) took %f seconds\n", iterations, time_taken);

    /* Verify first pixel of first card (100, 100) should be close to start_col */
    /* Verify last pixel of first card (100, 239) should be close to end_col */
    /* Verify middle pixel (100, 170) should be mix */

    uint32_t p_start = mock_fb[100 * MOCK_WIDTH + 100];
    uint32_t p_end = mock_fb[(100 + 139) * MOCK_WIDTH + 100];
    uint32_t p_mid = mock_fb[(100 + 70) * MOCK_WIDTH + 100];

    printf("Start Color (Expected ~Red): 0x%06X\n", p_start & 0xFFFFFF);
    printf("Mid Color (Expected ~Purple): 0x%06X\n", p_mid & 0xFFFFFF);
    printf("End Color (Expected ~Blue): 0x%06X\n", p_end & 0xFFFFFF);

    free(mock_fb);
}

int main() {
    benchmark_gradient();
    return 0;
}
