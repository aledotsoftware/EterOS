#include <framebuffer.h>
#include <string.h>

static uint32_t* fb_buffer = 0;
static uint32_t fb_width = 0;
static uint32_t fb_height = 0;
static uint32_t fb_pitch = 0;
static uint32_t fb_bpp = 0;

void framebuffer_init(boot_info_t* info) {
    if (!info || !info->fb_addr) return;

    fb_buffer = (uint32_t*)((uint64_t)info->fb_addr);
    fb_width = info->fb_width;
    fb_height = info->fb_height;
    fb_pitch = info->fb_pitch;
    fb_bpp = info->fb_bpp;
}

void framebuffer_putpixel(uint32_t x, uint32_t y, uint32_t color) {
    if (!fb_buffer || x >= fb_width || y >= fb_height) return;
    
    uint8_t* pixel_addr = (uint8_t*)fb_buffer + (y * fb_pitch) + (x * (fb_bpp / 8));

    if (fb_bpp == 32) {
        *(uint32_t*)pixel_addr = color;
    } 
    else if (fb_bpp == 24) {
        /* VBE suele ser BGR en Little Endian */
        pixel_addr[0] = (color) & 0xFF;       // Blue
        pixel_addr[1] = (color >> 8) & 0xFF;  // Green
        pixel_addr[2] = (color >> 16) & 0xFF; // Red
    }
    else if (fb_bpp == 16) {
        /* 5-6-5 RGB */
        uint8_t r = (color >> 16) & 0xFF;
        uint8_t g = (color >> 8) & 0xFF;
        uint8_t b = (color) & 0xFF;
        uint16_t c = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
        *(uint16_t*)pixel_addr = c;
    }
}

extern const uint8_t font8x16[];

void framebuffer_clear(uint32_t color) {
    if (!fb_buffer) return;

    /* Optimización para 32-bit */
    if (fb_bpp == 32) {
        for (uint32_t y = 0; y < fb_height; y++) {
            uint32_t* row = (uint32_t*)((uint8_t*)fb_buffer + (y * fb_pitch));
            for (uint32_t x = 0; x < fb_width; x++) {
                row[x] = color;
            }
        }
    } else {
        /* Genérico (lento pero seguro) */
        for (uint32_t y = 0; y < fb_height; y++) {
            for (uint32_t x = 0; x < fb_width; x++) {
                framebuffer_putpixel(x, y, color);
            }
        }
    }
}

void framebuffer_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    for (uint32_t i = 0; i < h; i++) {
        for (uint32_t j = 0; j < w; j++) {
            framebuffer_putpixel(x + j, y + i, color);
        }
    }
}

extern const uint8_t font8x16[];

void framebuffer_putchar(char c, uint32_t x, uint32_t y, uint32_t fg, uint32_t bg) {
    if (!fb_buffer) return;

    /* Usar unsigned char para acceder a los 256 caracteres del font estándar VGA */
    unsigned char uc = (unsigned char)c;

    /* Puntero al bitmap del caracter (16 bytes por char) */
    const uint8_t* glyph = &font8x16[uc * 16];

    for (int row = 0; row < 16; row++) {
        uint8_t bits = glyph[row];
        for (int col = 0; col < 8; col++) {
            /* Bit 7 es el pixel más a la izquierda */
            if (bits & (1 << (7 - col))) {
                framebuffer_putpixel(x + col, y + row, fg);
            } else {
                framebuffer_putpixel(x + col, y + row, bg);
            }
        }
    }
}
