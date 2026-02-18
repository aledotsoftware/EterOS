/*
 * éterOS - Desktop Environment (Eter OS Concept - Native Implementation)
 * 
 * Implementación de la nueva interfaz conceptual "Glassmorphism" con:
 * - Barra de estado unificada (Unified Header)
 * - Dock centrado estilo flotante
 * - Lanzador de aplicaciones en rejilla
 * - Ventanas con controles tipo macOS (Rojo/Amarillo/Verde)
 * - Fondo degradado radial
 */

#include <ui/window.h>
#include <ui/omni.h>
#include <task.h>
#include <types.h>
#include <string.h>
#include <vga.h>
#include <gui_demo.h>
#include <net/socket.h>
#include <net/defs.h>

#ifndef htons
#define htons(x) ((((x) & 0xff) << 8) | (((x) & 0xff00) >> 8))
#endif
#include <timer.h>
#include <keyboard.h>
#include <mouse.h>
#include <framebuffer.h>
#include <pci.h>
#include <shell.h>
#include <pmm.h>
#include <serial.h>
#include <ui/image.h>
#include <rtc.h>
#include <fs/initrd.h>
#include <syscall.h>
#include <hal.h>

extern int task_get_cpu_load(void);
extern int task_kill(uint32_t pid);
extern uint32_t my_ip; // Defined in stack.c

/* ========================================================================= */
/* Constantes Visuales (Eter OS Concept)                                     */
/* ========================================================================= */
#define STATUS_BAR_HEIGHT   44
#define DOCK_HEIGHT         64
#define DOCK_WIDTH          400
#define DOCK_BOTTOM_MARGIN  20

static volatile bool desktop_running = true;

static bool    mouse_left_btn = false;

/* Ventanas de Sistema */
static window_t* desktop_win = NULL;
static window_t* statusbar_win = NULL;
static window_t* dock_win = NULL;
static window_t* launcher_win = NULL;
static window_t* control_center_win = NULL;
/* Estado Global UI */
static bool control_center_active = false;
static char launcher_search[64] = "";

/* Paleta de Colores (CSS Variables ported to RGB Hex) */
#define CLR_BG_MAIN         0x0F172A
#define CLR_ACCENT          0x38BDF8
#define CLR_ACCENT_GLOW     0x38BDF8 // Alpha applied separately
#define CLR_LINUX           0x3B82F6
#define CLR_ANDROID         0x22C55E
#define CLR_TEXT_PRI        0xF8FAFC
#define CLR_TEXT_SEC        0x94A3B8
#define CLR_GLASS_BORDER    0xFFFFFF // Alpha 0.1 ~ 0x19
#define CLR_DOCK_BG         0xFFFFFF // Alpha 0.05 ~ 0x0D

/* ========================================================================= */
/* Helpers Graficos (Glassmorphism)                                          */
/* ========================================================================= */

static void draw_desktop_gradient(void) {
    uint32_t w = omni_get_width();
    uint32_t h = omni_get_height();
    
    // Radial Gradient Simulation: Center (Top-Left) to Outer
    // CSS: radial-gradient(circle at top left, #1e293b 0%, #0f172a 100%)
    // #1E293B (30, 41, 59) -> #0F172A (15, 23, 42)
    
    // Simplificacion: Fill solid bg first
    omni_fill_rect(0, 0, w, h, CLR_BG_MAIN);
}

/* ========================================================================= */
/* Unified Header (Status Bar)                                               */
/* ========================================================================= */

static void statusbar_on_paint(window_t* win) {
    (void)win;
    int w = omni_get_width();
    int h = STATUS_BAR_HEIGHT;

    // Glass background
    omni_fill_rect_alpha(0, 0, w, h, 0x0F172A, 0x66); // Darker glass
    omni_fill_rect_alpha(0, h-1, w, 1, 0xFFFFFF, 0x15); // Border bottom

    /* Left: Logo & App Name */
    omni_draw_string(NULL, 20, 14, "Eter OS", 0xFFFFFF, 0x00000000);
    
    // Unified Header: Mostrar controles si hay una ventana maximizada enfocada
    window_t* focused_window = wm_get_focused_window();
    if (focused_window && focused_window->maximized) {
        omni_draw_string(NULL, 100, 14, "|", CLR_TEXT_SEC, 0x000000);
        omni_draw_string(NULL, 120, 14, focused_window->title, CLR_TEXT_PRI, 0x000000);
        
        // Window Controls in Status Bar
        int cx = 300;
        omni_fill_rect(cx, 16, 12, 12, 0xFF5555); // Close
        omni_fill_rect(cx + 16, 16, 12, 12, 0x55FF55); // Max/Restore
        omni_fill_rect(cx + 32, 16, 12, 12, 0xFFFF55); // Min
    }

    /* Right: Control Center Toggles */
    // WiFi, Battery, Clock
    int rx = w - 20;

    // Clock
    static char clock_buf[16] = "00:00";
    rtc_time_t t;
    rtc_get_time(&t);
    int hr = (t.hours + 21) % 24; // Mock -3 GMT
    itoa_s(hr, clock_buf, 3, 10);
    strlcat(clock_buf, ":", 16);
    char min[3];
    itoa_s(t.minutes, min, 3, 10);
    if (t.minutes < 10) { clock_buf[3] = '0'; clock_buf[4] = 0; strlcat(clock_buf, min, 16); }
    else { clock_buf[3] = 0; strlcat(clock_buf, min, 16); }

    int cw = strlen(clock_buf) * 8;
    rx -= cw;
    omni_draw_string(NULL, rx, 14, clock_buf, CLR_TEXT_PRI, 0x000000);

    rx -= 20;
    // Battery Text
    rx -= 30;
    omni_draw_string(NULL, rx, 14, "88%", CLR_TEXT_PRI, 0x000000);

    rx -= 20;
    // WiFi Text
    rx -= 40;
    omni_draw_string(NULL, rx, 14, "WiFi 6", CLR_TEXT_PRI, 0x000000);
}

static void statusbar_on_mouse(window_t* win, int x, int y, int b) {
    (void)win; (void)y;
    int w = omni_get_width();
    
    // Unified Header Clicks
    window_t* focused_window = wm_get_focused_window();
    if (focused_window && focused_window->maximized) {
        if (b & 1 && !mouse_left_btn) {
            if (x >= 300 && x <= 312) { wm_destroy_window(focused_window); mouse_left_btn = true; return; }
            if (x >= 316 && x <= 328) { wm_restore_window(focused_window); mouse_left_btn = true; return; }
        }
    }

    if (b & 1 && !mouse_left_btn) {
        mouse_left_btn = true;
        if (x > w - 200) { // Right side -> Control Center
            if (control_center_active) {
                control_center_win->active = false;
                control_center_active = false;
            } else {
                control_center_win->active = true;
                wm_bring_to_front(control_center_win);
                control_center_active = true;
                // Hide launcher
                if (launcher_win->active) launcher_win->active = false;
            }
        }
    } else if (!(b & 1)) {
        mouse_left_btn = false;
    }
}

/* ========================================================================= */
/* Dock (Centered)                                                           */
/* ========================================================================= */

static void dock_on_paint(window_t* win) {
    (void)win;
    int dock_w = DOCK_WIDTH;
    int dock_h = DOCK_HEIGHT;
    
    // Glass Background
    omni_fill_rect_alpha(0, 0, dock_w, dock_h, 0xFFFFFF, 0x0D);
    omni_draw_rect(0, 0, dock_w, dock_h, 0x333333); // Border
    
    // Icons
    int icon_size = 48;
    int gap = 16;
    int start_x = (dock_w - (4 * icon_size + 3 * gap)) / 2;
    int cy = (dock_h - icon_size) / 2;
    
    // 1. Launcher
    omni_fill_rect_alpha(start_x, cy, icon_size, icon_size, 0xFFFFFF, 0x1A);
    ui_draw_image("icon_launcher.png", start_x, cy);
    
    // 2. GIMP (Linux)
    start_x += icon_size + gap;
    omni_fill_rect_alpha(start_x, cy, icon_size, icon_size, CLR_LINUX, 0x40);
    ui_draw_image("icon_gimp.png", start_x, cy);
    
    // 3. Play Store (Android)
    start_x += icon_size + gap;
    omni_fill_rect_alpha(start_x, cy, icon_size, icon_size, CLR_ANDROID, 0x40);
    ui_draw_image("icon_play.png", start_x, cy);

    // 4. Terminal
    start_x += icon_size + gap;
    omni_fill_rect_alpha(start_x, cy, icon_size, icon_size, 0x000000, 0x80);
    ui_draw_image("icon_term.png", start_x, cy);
}

static void dock_on_mouse(window_t* win, int x, int y, int b) {
    (void)win; (void)y;
    int dock_w = DOCK_WIDTH;

    if (b & 1 && !mouse_left_btn) {
        mouse_left_btn = true;
        
        // Simple hit test for Launcher (first icon area approx)
        int icon_size = 48;
        int gap = 16;
        int start_x = (dock_w - (4 * icon_size + 3 * gap)) / 2;
        
        if (x >= start_x && x <= start_x + icon_size) {
            // Launcher Toggle
            if (launcher_win->active) {
                launcher_win->active = false;
            } else {
                launcher_win->active = true;
                wm_bring_to_front(launcher_win);
                // Hide Control Center
                if (control_center_win->active) control_center_win->active = false;
            }
        }
    } else if (!(b & 1)) {
        mouse_left_btn = false;
    }
}

/* ========================================================================= */
/* Control Center (Overlay)                                                  */
/* ========================================================================= */

static void control_center_on_paint(window_t* win) {
    (void)win;
    int w = 320;
    int h = 400;

    // Glass BG
    omni_fill_rect_alpha(0, 0, w, h, 0x0F172A, 0xCC);
    omni_draw_rect(0, 0, w, h, 0x333333);

    // Header
    omni_draw_string(NULL, 20, 20, "Ajustes Rapidos", CLR_TEXT_PRI, 0x000000);
    omni_draw_string(NULL, w - 100, 22, "Martes, 17 Feb", CLR_TEXT_SEC, 0x000000);

    // Toggles Grid (2x2)
    int tx = 20;
    int ty = 50;
    int tw = (w - 50) / 2;
    int th = 60;

    // WiFi (Active)
    omni_fill_rect(tx, ty, tw, th, CLR_ACCENT);
    omni_draw_string(NULL, tx + 10, ty + 40, "WiFi", 0x000000, CLR_ACCENT);

    // BT (Active)
    omni_fill_rect(tx + tw + 10, ty, tw, th, CLR_ACCENT);
    omni_draw_string(NULL, tx + tw + 20, ty + 40, "BT", 0x000000, CLR_ACCENT);

    // Sliders
    int sy = ty + th + 20;
    omni_draw_string(NULL, tx, sy, "Brillo", CLR_TEXT_SEC, 0x000000);
    omni_fill_rect(tx, sy + 20, w - 40, 6, 0x333333);
    omni_fill_rect(tx, sy + 20, (w - 40) * 80 / 100, 6, 0xFFFFFF);

    // Notifications
    int ny = sy + 50;
    omni_draw_string(NULL, tx, ny, "Notificaciones", CLR_TEXT_PRI, 0x000000);

    // Notif 1
    omni_fill_rect_alpha(tx, ny + 20, w - 40, 50, 0xFFFFFF, 0x10);
    omni_draw_string(NULL, tx + 10, ny + 30, "Sistema", CLR_TEXT_PRI, 0x000000);
    omni_draw_string(NULL, tx + 10, ny + 50, "GIMP - Update Available (Linux)", CLR_TEXT_SEC, 0x000000);
}

static void control_center_on_mouse(window_t* win, int x, int y, int b) {
    (void)win; (void)x; (void)y;
    // Simple dismissal if clicked outside logic is handled by global desktop click
    // Or if clicking inside, handle toggles.
    if (b & 1 && !mouse_left_btn) {
        mouse_left_btn = true;
        // Handle clicks...
    } else if (!(b & 1)) {
        mouse_left_btn = false;
    }
}

/* ========================================================================= */
/* Launcher Overlay                                                          */
/* ========================================================================= */

static void launcher_on_paint(window_t* win) {
    (void)win;
    int sw = omni_get_width();
    int sh = omni_get_height();

    // Full screen blur dark (Draw over existing desktop)
    // Note: Since this is a window on top, we just fill our rect (which is full screen)
    omni_fill_rect_alpha(0, 0, sw, sh, 0x0F172A, 0xD0);

    // Search Bar
    int search_w = 600;
    int search_h = 50;
    int sx = (sw - search_w) / 2;
    int sy = 100;

    omni_fill_rect_alpha(sx, sy, search_w, search_h, 0xFFFFFF, 0x10);
    omni_draw_rect(sx, sy, search_w, search_h, 0x444444);
    if (strlen(launcher_search) > 0)
        omni_draw_string(NULL, sx + 20, sy + 18, launcher_search, 0xFFFFFF, 0x000000);
    else
        omni_draw_string(NULL, sx + 20, sy + 18, "Buscar aplicaciones (Linux, Android...)", 0x888888, 0x000000);

    // Grid
    int gx = sx;
    int gy = sy + 80;
    int item_w = 110;
    int item_h = 110;
    int cols = 6;
    
    const char* apps[] = {
        "Firefox", "VLC", "VS Code", "LibreOffice", "GIMP", "Terminal",
        "Spotify", "Chrome", "Play Store", "Instagram", "WhatsApp", "Gmail", "Maps", "YouTube"
    };
    uint32_t tags[] = {
        CLR_LINUX, CLR_LINUX, CLR_LINUX, CLR_LINUX, CLR_LINUX, CLR_LINUX,
        CLR_ANDROID, CLR_ANDROID, CLR_ANDROID, CLR_ANDROID, CLR_ANDROID, CLR_ANDROID, CLR_ANDROID, CLR_ANDROID
    };
    char icons[] = {
        'F', 'V', 'C', 'L', 'G', '>',
        'S', 'C', 'P', 'I', 'W', 'M', '@', 'Y'
    };
    
    for (int i = 0; i < 14; i++) {
        int col = i % cols;
        int row = i / cols;
        int x = gx + col * (item_w + 10);
        int y = gy + row * (item_h + 10);

        // Icon Bg
        omni_fill_rect_alpha(x + 25, y + 10, 60, 60, 0xFFFFFF, 0x15);
        omni_draw_char_scaled(x + 40, y + 22, icons[i], 0xFFFFFF, 2);

        // Tag
        omni_fill_rect(x + 25, y + 75, 50, 4, tags[i]);

        // Name
        omni_draw_string(NULL, x + 15, y + 85, apps[i], 0xFFFFFF, 0x000000);
    }
}

static void app_window_on_mouse(window_t* win, int x, int y, int b) {
    if (win->maximized) return; // Controls handled by Status Bar
    
    if (b & 1 && !mouse_left_btn) {
        mouse_left_btn = true;
        
        // Title bar hit test
        if (y < 30) {
            int btn_x = win->bounds.w - 20;
            if (x >= btn_x && x <= btn_x + 12) { wm_destroy_window(win); return; }
            if (x >= btn_x - 16 && x <= btn_x - 4) { wm_maximize_window(win); return; }
        }
    } else if (!(b & 1)) {
        mouse_left_btn = false;
    }
}

static void launcher_on_mouse(window_t* win, int x, int y, int b) {
    (void)win; 
    int sw = omni_get_width();

    if (b & 1 && !mouse_left_btn) {
        mouse_left_btn = true;
        
        // Grid Calculations (Must match on_paint)
        int search_w = 600;
        int sx = (sw - search_w) / 2;
        int sy = 100;
        
        int gx = sx;
        int gy = sy + 80;
        int item_w = 110;
        int item_h = 110;
        int cols = 6;
        
        // Check grid clicks
        if (x >= gx && y >= gy) {
            int rel_x = x - gx;
            int rel_y = y - gy;
            
            int col = rel_x / (item_w + 10);
            int row = rel_y / (item_h + 10);
            
            // Check if inside item bounds (ignoring gap roughly)
            int offset_x = rel_x % (item_w + 10);
            int offset_y = rel_y % (item_h + 10);
            
            if (offset_x < item_w && offset_y < item_h) {
                int index = row * cols + col;
                
                if (index >= 0 && index < 14) {
                    // Launch App!
                    const char* apps[] = {
                        "Firefox", "VLC", "VS Code", "LibreOffice", "GIMP", "Terminal",
                        "Spotify", "Chrome", "Play Store", "Instagram", "WhatsApp", "Gmail", "Maps", "YouTube"
                    };
                    
                    // Close launcher
                    launcher_win->active = false;
                    
                    // Spawn Window
                    // Position cascaded? Random? Centered?
                    static int spawn_x = 100;
                    static int spawn_y = 100;
                    
                    window_t* new_win = wm_create_window(spawn_x, spawn_y, 800, 600, apps[index]);
                    if (new_win) {
                        new_win->on_mouse = app_window_on_mouse;
                        // Determine Color based on app type?
                        // For now default dark theme
                        new_win->bg_color = 0x1E1E1E; 
                    }
                    
                    spawn_x += 30;
                    spawn_y += 30;
                    if (spawn_x > 400) spawn_x = 50;
                    if (spawn_y > 300) spawn_y = 50;
                    
                    return;
                }
            }
        }

        // If click far outside content, close?
        if (y < 80 || y > 600) {
            launcher_win->active = false;
        }
    } else if (!(b & 1)) {
        mouse_left_btn = false;
    }
}

/* ========================================================================= */
/* Desktop (Bottom Layer)                                                    */
/* ========================================================================= */

static void desktop_on_paint(window_t* win) {
    (void)win;
    draw_desktop_gradient();
}

static void desktop_on_mouse(window_t* win, int x, int y, int b) {
    (void)win; (void)x; (void)y;
    // Clicking desktop dismisses overlays
    if (b & 1 && !mouse_left_btn) {
        mouse_left_btn = true;
        if (launcher_win->active) launcher_win->active = false;
        if (control_center_win->active) control_center_win->active = false;
    } else if (!(b & 1)) {
        mouse_left_btn = false;
    }
}

/* ========================================================================= */
/* Main Entry Point                                                          */
/* ========================================================================= */

void gui_demo_run(void) {
    serial_write_string("[GUI] Starting Eter OS Concept UI...\n");
    
    desktop_running = true;
    wm_init();
    omni_init();
    framebuffer_enable_double_buffer();
    
    /* 1. Desktop Window (Bottom Layer, Full Screen) */
    desktop_win = wm_create_window(0, 0, 1024, 768, "Desktop");
    desktop_win->bg_color = CLR_BG_MAIN;
    desktop_win->on_paint = desktop_on_paint;
    desktop_win->on_mouse = desktop_on_mouse;
    
    /* 2. Status Bar (Top, 44px) */
    statusbar_win = wm_create_window(0, 0, 1024, STATUS_BAR_HEIGHT, "StatusBar");
    statusbar_win->bg_color = 0x000000; // Handled by paint
    statusbar_win->on_paint = statusbar_on_paint;
    statusbar_win->on_mouse = statusbar_on_mouse;

    /* 3. Dock (Bottom, Centered) */
    int dock_x = (1024 - DOCK_WIDTH) / 2;
    int dock_y = 768 - DOCK_BOTTOM_MARGIN - DOCK_HEIGHT;
    dock_win = wm_create_window(dock_x, dock_y, DOCK_WIDTH, DOCK_HEIGHT, "Dock");
    dock_win->bg_color = 0x000000;
    dock_win->on_paint = dock_on_paint;
    dock_win->on_mouse = dock_on_mouse;

    /* 4. Overlays (Hidden by default) */
    launcher_win = wm_create_window(0, 0, 1024, 768, "Launcher");
    launcher_win->active = false; // Hidden
    launcher_win->on_paint = launcher_on_paint;
    launcher_win->on_mouse = launcher_on_mouse;

    control_center_win = wm_create_window(1024 - 320 - 10, STATUS_BAR_HEIGHT + 10, 320, 400, "ControlCenter");
    control_center_win->active = false; // Hidden
    control_center_win->on_paint = control_center_on_paint;
    control_center_win->on_mouse = control_center_on_mouse;

    /* 5. Demo Apps */
    window_t* w1 = wm_create_window(100, 100, 600, 400, "GIMP (Linux)");
    if (w1) w1->on_mouse = app_window_on_mouse;
    (void)w1;
    
    while (desktop_running) {
        wm_pump_events();
        task_yield();
    }
}
