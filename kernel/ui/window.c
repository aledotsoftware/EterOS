/*
 * éterOS - Window Manager (Compositor)
 * 
 * Fase 5.1 Optimizations:
 * - Removed redundant buffer refreshes from per-operation calls
 * - Window shadow uses alpha blending for glass-like effect
 * - Improved title bar aesthetics with gradient
 * - Optimized clipping in wm_fill_rect / wm_print_at
 */
#include <ui/window.h>
#include <types.h>
#include <string.h>
#include <framebuffer.h>
#include <ui/omni.h>

static window_t current_windows[MAX_WINDOWS];
static int window_count = 0;

void wm_init(void) {
    memset(current_windows, 0, sizeof(current_windows));
    window_count = 0;
}

window_t* wm_create_window(int32_t x, int32_t y, int32_t w, int32_t h, const char* title) {
    if (window_count >= MAX_WINDOWS) return NULL;
    
    window_t* win = &current_windows[window_count++];
    win->active = true;
    win->bounds.x = x;
    win->bounds.y = y;
    win->bounds.w = w;
    win->bounds.h = h;
    win->bg_color = UI_COLOR_DARK;
    win->fg_color = UI_COLOR_WHITE;
    strlcpy(win->title, title, sizeof(win->title));
    
    /* Dibujar inicialmente */
    wm_draw_window(win);
    return win;
}

void wm_draw_window(window_t* win) {
    if (!win->active) return;
    
    const int TITLE_H = 30;
    
    /* 0. Shadow (Soft, offset +3, +3) */
    omni_fill_rect_alpha(win->bounds.x + 3, win->bounds.y + 3,
                   win->bounds.w, win->bounds.h,
                   0x000000, 0x80);

    /* 1. Background */
    omni_fill_rect(win->bounds.x, win->bounds.y,
                   win->bounds.w, win->bounds.h,
                   win->bg_color);
                     
    /* 2. Title Bar (Gradient for premium feel) */
    omni_fill_gradient_v(win->bounds.x, win->bounds.y,
                         win->bounds.w, TITLE_H,
                         0x00BBCC, UI_COLOR_CYAN);
                     
    /* Title Text (Centrado verticalmente en 30px) */
    omni_draw_string(NULL, win->bounds.x + 8, win->bounds.y + 7, win->title, UI_COLOR_BLACK, UI_COLOR_CYAN);
    
    /* 3. Close Button [X] (Touch-friendly 20x20) */
    int btn_size = 20;
    int btn_margin = (TITLE_H - btn_size) / 2;
    int btn_x = win->bounds.x + win->bounds.w - btn_size - btn_margin;
    int btn_y = win->bounds.y + btn_margin;
    
    omni_fill_rect(btn_x, btn_y, btn_size, btn_size, 0xFF4040);
    
    /* Draw X with line primitives (faster than per-pixel) */
    omni_draw_line(btn_x + 4, btn_y + 4, btn_x + 15, btn_y + 15, 0xFFFFFF);
    omni_draw_line(btn_x + 15, btn_y + 4, btn_x + 4, btn_y + 15, 0xFFFFFF);
    
    /* 4. Border (Single pixel) */
    omni_draw_rect(win->bounds.x, win->bounds.y, win->bounds.w, win->bounds.h, 0x505050);
}

void wm_draw_all(void) {
    for (int i = 0; i < window_count; i++) {
        wm_draw_window(&current_windows[i]);
    }
}

/* Draws text relative to window content area (below title bar) */
void wm_print_at(window_t* win, int32_t x, int32_t y, const char* text) {
    if (!win || !win->active) return;

    const int TITLE_H = 30;

    int32_t abs_x = win->bounds.x + x;
    int32_t abs_y = win->bounds.y + TITLE_H + y;
    
    /* Clipping Rect: Content Area */
    rect_t clip;
    clip.x = win->bounds.x;
    clip.y = win->bounds.y + TITLE_H;
    clip.w = win->bounds.w;
    clip.h = win->bounds.h - TITLE_H;

    omni_draw_string(&clip, abs_x, abs_y, text, win->fg_color, win->bg_color);
}

void wm_fill_rect(window_t* win, rect_t rect, uint32_t color) {
    if (!win || !win->active) return;
    
    const int TITLE_H = 30;
    
    /* Offset for title bar */
    int32_t abs_x = win->bounds.x + rect.x;
    int32_t abs_y = win->bounds.y + TITLE_H + rect.y;

    /* Content area bounds */
    int32_t min_x = win->bounds.x;
    int32_t min_y = win->bounds.y + TITLE_H;
    int32_t max_x = win->bounds.x + win->bounds.w;
    int32_t max_y = win->bounds.y + win->bounds.h;

    /* Clip */
    if (abs_x < min_x) {
        rect.w -= (min_x - abs_x);
        abs_x = min_x;
    }
    if (abs_y < min_y) {
        rect.h -= (min_y - abs_y);
        abs_y = min_y;
    }

    if (abs_x + rect.w > max_x) {
        rect.w = max_x - abs_x;
    }
    if (abs_y + rect.h > max_y) {
        rect.h = max_y - abs_y;
    }

    if (rect.w <= 0 || rect.h <= 0) return;
    
    omni_fill_rect(abs_x, abs_y, rect.w, rect.h, color);
}

void wm_move_window(window_t* win, int32_t dx, int32_t dy) {
    if (!win->active) return;
    win->bounds.x += dx;
    win->bounds.y += dy;
    
    wm_redraw_desktop();
}

void wm_redraw_desktop(void) {
    omni_begin_frame();
    
    /* 1. Desktop Background (Dark Teal) */
    omni_fill_rect(0, 0, omni_get_width(), omni_get_height(), 0x002040);
    
    /* 2. Subtle Grid */
    for (uint32_t y = 0; y < omni_get_height(); y += 40) {
        omni_fill_rect(0, y, omni_get_width(), 1, 0x003050);
    }
    for (uint32_t x = 0; x < omni_get_width(); x += 40) {
        omni_fill_rect(x, 0, 1, omni_get_height(), 0x003050);
    }

    /* 3. Redraw all windows */
    wm_draw_all();
    
    /* 4. Flush */
    framebuffer_flush();
}
