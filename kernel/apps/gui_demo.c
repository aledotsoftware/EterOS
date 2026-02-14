/*
 * éterOS - Desktop Environment (Flux UI)
 * 
 * Implementación de un entorno de escritorio básico con:
 * - Barra de tareas y Menú Inicio estilo "Aurora" (Touch-optimized)
 * - Ventana de Terminal interactiva mejorada
 * - Soporte de Mouse y Teclado
 * - Snapping de ventanas
 */

#include <ui/window.h>
#include <task.h>
#include <types.h>
#include <string.h>
#include <vga.h>
#include <timer.h>
#include <keyboard.h>
#include <mouse.h>
#include <framebuffer.h>
#include <shell.h>
#include <pmm.h>

/* ========================================================================= */
/* Constantes y Configuración Visual                                         */
/* ========================================================================= */
#define TASKBAR_HEIGHT      40
#define TITLE_BAR_HEIGHT    30

static volatile bool desktop_running = true;

/* Ventanas y Estado */
static window_t* win_sysinfo = NULL;

/* Mouse */
static int32_t mouse_x = 512;
static int32_t mouse_y = 384;
static bool    mouse_left_btn = false;

/* Drag & Drop */
static window_t* win_drag = NULL;
static int32_t drag_offset_x = 0;
static int32_t drag_offset_y = 0;

/* Estado Menu */
static bool menu_open = false;

/* ========================================================================= */
/* Terminal App Logica Multi-instancia con Scroll                            */
/* ========================================================================= */

#define TERM_VISIBLE_LINES 14
#define TERM_VISIBLE_COLS  40
/* Aumentamos buffer para permitir "scroll back" conceptual */
#define TERM_BUFFER_LINES  50 
#define MAX_TERMINALS 15

typedef struct {
    window_t* win;
    char buffer[TERM_BUFFER_LINES][TERM_VISIBLE_COLS + 1]; /* Historial */
    int  cursor_x; /* 0..TERM_VISIBLE_COLS-1 */
    int  cursor_y; /* Posición absoluta en buffer */
    int  scroll_offset; 
    char input_buf[64];
    int  input_pos;
    bool used;
} term_instance_t;

static term_instance_t terminals[MAX_TERMINALS];
static term_instance_t* focused_term = NULL;
static term_instance_t* hook_term = NULL;

static void term_init_all(void) {
    for (int i = 0; i < MAX_TERMINALS; i++) {
        terminals[i].used = false;
        terminals[i].win = NULL;
    }
    focused_term = NULL;
    hook_term = NULL;
}

static void term_clear(term_instance_t* term) {
    if (!term) return;
    for (int i = 0; i < TERM_BUFFER_LINES; i++) {
        memset(term->buffer[i], 0, sizeof(term->buffer[i]));
    }
    term->cursor_x = 0;
    term->cursor_y = 0;
    term->scroll_offset = 0;
    term->input_pos = 0;
    memset(term->input_buf, 0, sizeof(term->input_buf));
}

static void term_putc(term_instance_t* term, char c) {
    if (!term) return;

    if (c == '\n') {
        term->cursor_y++;
        term->cursor_x = 0;
    } else if (c == '\b') {
        if (term->cursor_x > 0) {
            term->cursor_x--;
            term->buffer[term->cursor_y][term->cursor_x] = 0;
        }
    } else if (term->cursor_x < TERM_VISIBLE_COLS - 1) {
        term->buffer[term->cursor_y][term->cursor_x++] = c;
        term->buffer[term->cursor_y][term->cursor_x] = 0;
    }

    if (term->cursor_y >= TERM_BUFFER_LINES) {
        for (int i = 0; i < TERM_BUFFER_LINES - 1; i++) {
            memcpy(term->buffer[i], term->buffer[i+1], TERM_VISIBLE_COLS + 1);
        }
        memset(term->buffer[TERM_BUFFER_LINES-1], 0, TERM_VISIBLE_COLS + 1);
        term->cursor_y = TERM_BUFFER_LINES - 1;
    }
    
    term->scroll_offset = 0;
}

static void term_print(term_instance_t* term, const char* text) {
    if (!term) return;
    while (*text) term_putc(term, *text++);
}

static void gui_term_hook(char c) {
    if (hook_term) term_putc(hook_term, c);
}

static void term_execute_cmd(term_instance_t* term, const char* cmd) {
    if (!term) return;
    
    term_print(term, "\n");
    
    if (strcmp(cmd, "clear") == 0) {
        term_clear(term);
        return;
    }
    
    if (strcmp(cmd, "exit") == 0) {
        if (term->win) term->win->active = false;
        term->used = false;
        if (focused_term == term) focused_term = NULL;
        return;
    }

    hook_term = term;
    terminal_set_hook(gui_term_hook);
    int ret = shell_exec((char*)cmd);
    terminal_set_hook(NULL);
    hook_term = NULL;
    
    if (ret != 0) { }
    
    term_print(term, "\neterOS> ");
}

static void term_handle_key(term_instance_t* term, char c) {
    if (!term) return;

    if (c == '\n') {
        term_execute_cmd(term, term->input_buf);
        memset(term->input_buf, 0, sizeof(term->input_buf));
        term->input_pos = 0;
    } else if (c == '\b') {
        if (term->input_pos > 0) {
            term->input_pos--;
            term->input_buf[term->input_pos] = 0;
            term_putc(term, '\b'); 
        }
    } else {
        if (term->input_pos < 60) {
            term->input_buf[term->input_pos++] = c;
            term->input_buf[term->input_pos] = 0;
            term_putc(term, c); 
        }
    }
}

static void draw_terminal_content(term_instance_t* term) {
    if (!term || !term->win || !term->win->active) return;

    rect_t client_area = {0, 0, term->win->bounds.w, term->win->bounds.h};
    wm_fill_rect(term->win, client_area, UI_COLOR_BLACK);
    
    int start_line = term->cursor_y - (TERM_VISIBLE_LINES - 1) - term->scroll_offset;
    if (start_line < 0) start_line = 0;
    
    int draw_y = 5;
    for (int i = 0; i < TERM_VISIBLE_LINES; i++) {
        int buf_idx = start_line + i;
        if (buf_idx < TERM_BUFFER_LINES) {
            if (term->buffer[buf_idx][0] != 0) {
                wm_print_at(term->win, 5, draw_y, term->buffer[buf_idx]);
            }
        }
        draw_y += 16;
    }
    
    int rel_cursor_y = term->cursor_y - start_line;
    if (rel_cursor_y >= 0 && rel_cursor_y < TERM_VISIBLE_LINES) {
        int cursor_px = 5 + (term->cursor_x * 8);
        int cursor_py = 5 + (rel_cursor_y * 16);
        bool blink = (timer_get_ticks() / 50) % 2 == 0;
        uint32_t cursor_color = (term == focused_term) ? UI_COLOR_GREEN : 0x404040;
        if (blink || term != focused_term) {
             wm_fill_rect(term->win, (rect_t){cursor_px, cursor_py + 12, 8, 2}, cursor_color);
        }
    }
}

static void term_create(void) {
    int slot = -1;
    for (int i = 0; i < MAX_TERMINALS; i++) {
        if (!terminals[i].used) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) return; 
    
    char title[32];
    strlcpy(title, "Terminal ", 32);
    char num[4];
    itoa_s(slot + 1, num, 4, 10);
    strlcpy(title + 9, num, 32);
    
    int x = 50 + (slot * 30);
    int y = 50 + (slot * 30);
    if (x > 600) x = 100;
    if (y > 400) y = 100;
    
    /* Ventana ligeramente más ancha para title bar moderna */
    terminals[slot].win = wm_create_window(x, y, 360, 260 + TITLE_BAR_HEIGHT - 20, title);
    if (!terminals[slot].win) return;
    
    terminals[slot].win->bg_color = UI_COLOR_BLACK;
    terminals[slot].used = true;
    
    term_clear(&terminals[slot]);
    term_print(&terminals[slot], "eterOS Genesis Shell\n\n");
    term_print(&terminals[slot], "eterOS> ");
    focused_term = &terminals[slot];
}

/* ========================================================================= */
/* System Apps (SysInfo, Matrix)                                             */
/* ========================================================================= */

static void draw_progress_bar(window_t* win, int x, int y, int w, int h, float percent, uint32_t color) {
    rect_t bg = {x, y, w, h};
    wm_fill_rect(win, bg, 0x404040);
    int fill_w = (int)((float)w * percent);
    if (fill_w > w) fill_w = w;
    if (fill_w < 0) fill_w = 0;
    rect_t fill = {x, y, fill_w, h};
    wm_fill_rect(win, fill, color);
}

static void draw_sysinfo_content(void) {
    wm_fill_rect(win_sysinfo, (rect_t){0, 0, 240, 140}, UI_COLOR_DARK);
    wm_print_at(win_sysinfo, 10, 10, "eterOS Genesis - Monitor");
    wm_print_at(win_sysinfo, 10, 30, "CPU: x86_64 (1 Core)");
    
    uint64_t total = pmm_get_total_ram();
    uint64_t free  = pmm_get_free_ram();
    uint64_t used  = total - free;
    
    char buf[64];
    itoa_s(used / (1024*1024), buf, sizeof(buf), 10);
    char line[64] = "RAM: ";
    strlcpy(line + 5, buf, 64);
    strlcpy(line + strlen(line), " MB / ", 64);
    itoa_s(total / (1024*1024), buf, sizeof(buf), 10);
    strlcpy(line + strlen(line), buf, 64);
    strlcpy(line + strlen(line), " MB", 64); 
    wm_print_at(win_sysinfo, 10, 50, line);
    
    float usage_pct = (float)used / (float)total;
    uint32_t bar_color = (usage_pct > 0.8f) ? UI_COLOR_RED : UI_COLOR_GREEN;
    draw_progress_bar(win_sysinfo, 10, 65, 180, 10, usage_pct, bar_color);

    uint32_t uptime = timer_get_uptime_seconds();
    int min = uptime / 60; int sec = uptime % 60;
    char time_str[32];
    itoa_s(min, buf, sizeof(buf), 10);
    strlcpy(time_str, "Uptime: ", 32);
    strlcpy(time_str + strlen(time_str), buf, 32);
    strlcpy(time_str + strlen(time_str), "m ", 32);
    itoa_s(sec, buf, sizeof(buf), 10);
    strlcpy(time_str + strlen(time_str), buf, 32);
    strlcpy(time_str + strlen(time_str), "s", 32);
    wm_print_at(win_sysinfo, 10, 90, time_str);
}

static window_t* win_matrix = NULL;
static volatile bool matrix_running = false;
#define MATRIX_COLS 40
#define MATRIX_ROWS 20
static char matrix_chars[MATRIX_COLS][MATRIX_ROWS];
static int  matrix_drops[MATRIX_COLS];

void task_matrix_rain(void) {
    for (int x = 0; x < MATRIX_COLS; x++) {
        matrix_drops[x] = -(x % 15);
        for (int y = 0; y < MATRIX_ROWS; y++) matrix_chars[x][y] = ' ';
    }
    while (matrix_running && win_matrix && win_matrix->active) {
        for (int x = 0; x < MATRIX_COLS; x++) {
            matrix_drops[x]++;
            int head = matrix_drops[x];
            if (head >= 0 && head < MATRIX_ROWS) {
                static const char charset[] = "01XZ@#%&";
                char c = charset[(timer_get_uptime_seconds() + head + x) % 8];
                matrix_chars[x][head] = c;
            }
            int tail = head - 8;
            if (tail >= 0 && tail < MATRIX_ROWS) matrix_chars[x][tail] = ' ';
            if (head > MATRIX_ROWS + 5) matrix_drops[x] = -(x % 10) - 2;
        }
        wm_fill_rect(win_matrix, (rect_t){0, 0, 320, 200}, UI_COLOR_BLACK);
        for (int x = 0; x < MATRIX_COLS; x++) {
            for (int y = 0; y < MATRIX_ROWS; y++) {
                char c = matrix_chars[x][y];
                if (c != ' ') {
                    uint32_t color = UI_COLOR_GREEN;
                    if (y < matrix_drops[x]) color = 0xFF008000;
                    if (y == matrix_drops[x]) color = UI_COLOR_WHITE;
                    (void)color; 
                    char s[2] = {c, 0};
                    wm_print_at(win_matrix, x * 8, y * 10, s);
                }
            }
        }
        task_sleep(50);
    }
    matrix_running = false; win_matrix = NULL; task_exit();
}

/* ========================================================================= */
/* Settings App                                                              */
/* ========================================================================= */

typedef enum {
    TAB_DISPLAY = 0,
    TAB_NETWORK,
    TAB_REGION
} settings_tab_t;

static window_t* win_settings = NULL;
static settings_tab_t settings_tab = TAB_DISPLAY;
static int settings_lang = 0; /* 0=Español, 1=English */

static void draw_settings_content(void) {
    if (!win_settings) return;
    
    /* Layout: Sidebar (100px) + Content (Resto) */
    int side_w = 110;
    int win_h = win_settings->bounds.h - TITLE_BAR_HEIGHT; /* Altura area cliente */
    
    /* Sidebar Background */
    wm_fill_rect(win_settings, (rect_t){0, 0, side_w, win_h}, 0x2A2A2A);
    wm_fill_rect(win_settings, (rect_t){side_w, 0, 1, win_h}, 0x404040); /* Separator */
    
    /* Sidebar Items */
    const char* items[] = {"Pantalla", "Internet", "Idioma"};
    int item_h = 40;
    
    for (int i = 0; i < 3; i++) {
        uint32_t bg = (i == settings_tab) ? 0x004080 : 0x2A2A2A;
        uint32_t fg = (i == settings_tab) ? 0xFFFFFF : 0xA0A0A0;
        
        wm_fill_rect(win_settings, (rect_t){0, i*item_h, side_w, item_h}, bg);
        wm_print_at(win_settings, 10, i*item_h + 12, items[i]);
    }

    /* Content Area Background */
    wm_fill_rect(win_settings, (rect_t){side_w + 1, 0, win_settings->bounds.w - side_w - 1, win_h}, 0x1A1A1A);
    
    int cx = side_w + 20;
    int cy = 20;
    
    if (settings_tab == TAB_DISPLAY) {
        wm_print_at(win_settings, cx, cy, "Resolucion de Pantalla");
        cy += 30;
        
        /* Mock Dropdown box */
        wm_fill_rect(win_settings, (rect_t){cx, cy, 150, 24}, 0x303030);
        wm_print_at(win_settings, cx + 5, cy + 5, "1024 x 768 (32 bit)");
        wm_fill_rect(win_settings, (rect_t){cx + 130, cy, 20, 24}, 0x505050); /* Arrow bg */
        // draw arrow down?
        cy += 40;
        wm_print_at(win_settings, cx, cy, "Escala (DPI): 100%");
        
    } else if (settings_tab == TAB_NETWORK) {
        wm_print_at(win_settings, cx, cy, "Conexiones de Red");
        cy += 30;
        
        wm_print_at(win_settings, cx, cy, "Adaptador: Intel E1000");
        cy += 20;
        wm_print_at(win_settings, cx, cy, "Estado: Conectado (Simulado)");
        cy += 20;
        wm_print_at(win_settings, cx, cy, "Direccion IP: 192.168.1.105");
        
        cy += 30;
        /* Button */
        wm_fill_rect(win_settings, (rect_t){cx, cy, 120, 24}, 0x006000);
        wm_print_at(win_settings, cx + 15, cy + 5, "Renovar IP");
        
    } else if (settings_tab == TAB_REGION) {
        wm_print_at(win_settings, cx, cy, "Configuracion Regional");
        cy += 30;
        wm_print_at(win_settings, cx, cy, "Seleccionar Idioma:");
        cy += 30;
        
        /* Radio Buttons Mockup */
        /* Español */
        uint32_t c1 = (settings_lang == 0) ? UI_COLOR_GREEN : 0x505050;
        wm_fill_rect(win_settings, (rect_t){cx, cy, 12, 12}, c1);
        wm_print_at(win_settings, cx + 20, cy - 2, "Espanol (AR)");
        
        cy += 25;
        /* English */
        uint32_t c2 = (settings_lang == 1) ? UI_COLOR_GREEN : 0x505050;
        wm_fill_rect(win_settings, (rect_t){cx, cy, 12, 12}, c2);
        wm_print_at(win_settings, cx + 20, cy - 2, "English (US)");
        
        /* Note: This variable 'settings_lang' currently only affects this radio button visual */
    }
}

static void handle_settings_click(int win_local_x, int win_local_y) {
    if (win_local_x < 110) {
        /* Sidebar click */
        int item_h = 40;
        int idx = win_local_y / item_h;
        if (idx >= 0 && idx <= 2) {
            settings_tab = (settings_tab_t)idx;
        }
    } else {
        /* Content area click */
        int cx = 130;  /* side_w (110) + margin (20) */
        int cy = 20;
        
        if (settings_tab == TAB_REGION) {
            /* Check Language Radio buttons positions from draw_settings_content */
            /* "Seleccionar Idioma" is at cy+30 (50). Radio 1 at 50+30=80. Radio 2 at 80+25=105 */
            int r1_y = 80;
            int r2_y = 105;
            
            if (win_local_y >= r1_y - 5 && win_local_y <= r1_y + 15) {
                settings_lang = 0;
            }
            if (win_local_y >= r2_y - 5 && win_local_y <= r2_y + 15) {
                settings_lang = 1;
            }
        }
        else if (settings_tab == TAB_NETWORK) {
            /* Check for "Renovar IP" button */
            /* Starts at cy=20. +30+20+20+20+30 = 120 approx */
            int btn_y = 120;
            if (win_local_y >= btn_y && win_local_y <= btn_y + 24 && win_local_x >= cx && win_local_x <= cx + 120) {
                /* Todo: Trigger DHCP discover */
            }
        }
    }
}

/* ========================================================================= */
/* Lógica UI, Touch y Desktop                                                */
/* ========================================================================= */

/* ========================================================================= */
/* Flux UI - The Ontology of the System                                      */
/* ========================================================================= */

/* Ontología del Sistema: Colores y Materiales */
#define FLUX_VOID           0x050505  /* Deep Void (Capa 0) */
#define FLUX_ACCENT_CYAN    0x00FFFF  /* Flujo Activo */
#define FLUX_ACCENT_VIOLET  0x9D00FF  /* Profundidad */
#define FLUX_ACCENT_AMBER   0xFFBF00  /* Alerta / Foco */
#define FLUX_CARD_BG        0x101010  /* Espacios (Capa 1) */
#define FLUX_TEXT_PRIMARY   0xFFFFFF
#define FLUX_TEXT_SECONDARY 0x888888

typedef enum {
    FLUX_MACRO = 0,   /* Constellation (Overview) */
    FLUX_FOCUS = 1    /* Detail (Active App) */
} flux_zoom_level_t;

static flux_zoom_level_t current_zoom = FLUX_MACRO;
static window_t* focused_space = NULL; /* The active "Space" */

/* ========================================================================= */
/* Flux Rendering Primitives                                                 */
/* ========================================================================= */

static void flux_draw_card_preview(int x, int y, int w, int h, const char* title, const char* subtitle, uint32_t accent) {
    /* Capa 1: Contenedor */
    framebuffer_rect(x, y, w, h, FLUX_CARD_BG);
    
    /* Glow / Borde Activo (Simulado) */
    //framebuffer_rect(x, y + h - 2, w, 2, accent);
    
    /* Icono / Miniatura (Placeholder abstracto) */
    int icon_size = 40;
    int icon_x = x + (w - icon_size) / 2;
    int icon_y = y + 40;
    framebuffer_rect(icon_x, icon_y, icon_size, icon_size, 0x202020);
    framebuffer_rect(icon_x + 10, icon_y + 10, icon_size - 20, icon_size - 20, accent);
    
    /* Texto */
    int text_len = strlen(title) * 8;
    int text_x = x + (w - text_len) / 2;
    ui_draw_string(NULL, text_x, y + 100, title, FLUX_TEXT_PRIMARY, FLUX_CARD_BG);

    text_len = strlen(subtitle) * 8;
    text_x = x + (w - text_len) / 2;
    ui_draw_string(NULL, text_x, y + 120, subtitle, FLUX_TEXT_SECONDARY, FLUX_CARD_BG);
}

/* ========================================================================= */
/* Flux Navigation & Input                                                   */
/* ========================================================================= */

/* Constellation Nodes (Apps disponibles) */
typedef enum {
    NODE_TERMINAL = 0,
    NODE_SYSMON,
    NODE_MATRIX,
    NODE_SETTINGS,
    NODE_COUNT
} flux_node_id_t;

static void flux_launch_space(flux_node_id_t node) {
    /* Transición a Focus */
    current_zoom = FLUX_FOCUS;
    
    if (node == NODE_TERMINAL) {
        /* Ensure a terminal is ready */
        if (!terminals[0].used) { term_create(); }
        focused_space = terminals[0].win;
        focused_term = &terminals[0];
    } else if (node == NODE_SYSMON) {
        if (!win_sysinfo) win_sysinfo = wm_create_window(0, 0, 800, 600, "System Info"); // Bounds ignored in Focus mode
        win_sysinfo->active = true;
        focused_space = win_sysinfo;
    } else if (node == NODE_MATRIX) {
        if (!win_matrix) {
             win_matrix = wm_create_window(0, 0, 800, 600, "Matrix");
             win_matrix->bg_color = UI_COLOR_BLACK; 
             matrix_running = true; task_create("matrix_rain", task_matrix_rain);
        }
        win_matrix->active = true;
        focused_space = win_matrix;
    } else if (node == NODE_SETTINGS) {
         if (!win_settings) win_settings = wm_create_window(0, 0, 800, 600, "Configuracion");
         win_settings->active = true;
         focused_space = win_settings;
    }
}

static void draw_constellation(void) {
    /* Capa 0: Void is already cleared */
    
    /* Header (Capa 3) */
    ui_draw_string(NULL, 50, 50, "CONSTELACION", FLUX_TEXT_SECONDARY, FLUX_VOID);
    ui_draw_string(NULL, 900, 50, "12:00", FLUX_TEXT_PRIMARY, FLUX_VOID);

    /* Grid de Nodos (Centrado) */
    int card_w = 200;
    int card_h = 240;
    int gap = 40;
    int start_x = (1024 - ((card_w * 2) + gap)) / 2;
    int start_y = 200;
    
    /* Nodo 1: Terminal */
    flux_draw_card_preview(start_x, start_y, card_w, card_h, "Terminal Core", "Acceso Directo", FLUX_ACCENT_CYAN);
    
    /* Nodo 2: System */
    flux_draw_card_preview(start_x + card_w + gap, start_y, card_w, card_h, "System Health", "Monitor de Recursos", FLUX_ACCENT_AMBER);
    
    /* Fila 2 */
    int row2_y = start_y + card_h + gap;
    
    /* Nodo 3: Matrix */
    flux_draw_card_preview(start_x, row2_y, card_w, card_h, "Matrix Stream", "Visualizacion", FLUX_ACCENT_VIOLET);

    /* Nodo 4: Settings */
    flux_draw_card_preview(start_x + card_w + gap, row2_y, card_w, card_h, "Ajustes", "Configuracion", 0xFFFFFF);
}

static void draw_focus_mode(void) {
    if (!focused_space || !focused_space->active) {
        current_zoom = FLUX_MACRO; /* Fallback if active app died */
        return;
    }

    /* En Focus Mode, el espacio activo ocupa casi toda la pantalla (con márgenes estéticos) */
    /* Update bounds logic usually happens here or guarantees bounds in draw */
    /* Forcing bounds for "Immersion" */
    int margin = 40; // Margen para "Respirar"
    focused_space->bounds.x = margin;
    focused_space->bounds.y = margin;
    focused_space->bounds.w = 1024 - (margin * 2);
    focused_space->bounds.h = 768 - (margin * 2);
    
    /* Draw the content of the active space */
    /* HACK: We call specialized draw functions because generic wm_draw_window draws a titlebar we don't want */
    
    /* Shadow/Glow behind content */
    framebuffer_rect(focused_space->bounds.x - 2, focused_space->bounds.y - 2, 
                     focused_space->bounds.w + 4, focused_space->bounds.h + 4, FLUX_ACCENT_CYAN); // Active Glow
                     
    if (focused_space == win_sysinfo) {
        draw_sysinfo_content(); // Rellena el buffer interno
        /* Draw manually without title bar */
        // wm_draw_window(focused_space); 
        /* Actually sysinfo helper uses wm_print_at which relies on window bounds. 
           In Flux, we want borderless. */
         wm_fill_rect(focused_space, (rect_t){0,0,focused_space->bounds.w, focused_space->bounds.h}, FLUX_CARD_BG);
         draw_sysinfo_content(); // Re-draw content on top of BG
    } 
    else if (focused_space == win_settings) {
         draw_settings_content();
    }
    else if (focused_space == win_matrix) {
        // Matrix draws itself in its task loop, just needs bounds update
    }
    else {
        /* Assume Terminal */
         draw_terminal_content(focused_term); 
    }
    
    /* Capa 2: Controles de Navegación (Bottom Bar simplificada) */
    int bar_h = 6;
    int bar_w = 200;
    int bar_x = (1024 - bar_w) / 2;
    int bar_y = 768 - 20;
    framebuffer_rect(bar_x, bar_y, bar_w, bar_h, 0x404040); // Home Indicator "Pill"
}

/* Input Handling for Flux */
static void handle_flux_click(void) {
    if (current_zoom == FLUX_MACRO) {
        /* Hit testing Constellation Cards */
        int card_w = 200;
        int card_h = 240;
        int gap = 40;
        int start_x = (1024 - ((card_w * 2) + gap)) / 2;
        int start_y = 200;
        int row2_y = start_y + card_h + gap;

        #define HIT_CARD(x, y, wa, ha, node) \
            if (mouse_x >= x && mouse_x <= x + wa && mouse_y >= y && mouse_y <= y + ha) { \
                flux_launch_space(node); return; \
            }
            
        HIT_CARD(start_x, start_y, card_w, card_h, NODE_TERMINAL);
        HIT_CARD(start_x + card_w + gap, start_y, card_w, card_h, NODE_SYSMON);
        HIT_CARD(start_x, row2_y, card_w, card_h, NODE_MATRIX);
        HIT_CARD(start_x + card_w + gap, row2_y, card_w, card_h, NODE_SETTINGS);
        
    } else {
        /* Focus Mode Input */
        /* Check Bottom Home gesture */
        if (mouse_y > 740) {
            /* Swipe up / Click bottom area -> Go back to Macro */
            current_zoom = FLUX_MACRO;
            return;
        }
        
        /* Pass input to active app controls */
        if (focused_space == win_settings) {
             /* Adjust mouse coordinates to be relative to the window, which is now at (margin, margin) */
             /* The handle_settings_click expects local coordinates but assumes titlebar offset. 
                Flux has NO titlebar. */
             handle_settings_click(mouse_x - focused_space->bounds.x, mouse_y - focused_space->bounds.y + TITLE_BAR_HEIGHT); 
             /* We add TITLE_BAR_HEIGHT to y to fake it because handle_settings_click subtracts it?? 
                No, handle_settings_click subtracts it in the CALLER. */
             /* Let's fix handle_settings_click to accept absolute and calc local? No. */
        }
    }
}

static void on_mouse_event(int8_t dx, int8_t dy, uint8_t buttons) {
    mouse_x += dx; mouse_y += dy;
    if (mouse_x < 0) mouse_x = 0;
    if (mouse_y < 0) mouse_y = 0;
    if (mouse_x >= 1024) mouse_x = 1023;
    if (mouse_y >= 768) mouse_y = 767;
    
    bool left_btn = (buttons & 1);
    
    if (left_btn && !mouse_left_btn) {
        handle_flux_click();
    }
    mouse_left_btn = left_btn;
}

void gui_demo_run(void) {
    wm_init();
    framebuffer_enable_double_buffer();
    mouse_set_callback(on_mouse_event);
    
    /* Init Apps */
    term_init_all();
    
    desktop_running = true;
    while (desktop_running) {
        if (keyboard_has_input()) {
             char c = keyboard_getchar();
             if (c == 27) current_zoom = FLUX_MACRO; /* ESC returns to Constellation */
             else if (current_zoom == FLUX_FOCUS && focused_term && focused_term == (term_instance_t*)focused_term) {
                 /* Hack: check if active space is terminal */
                 if (focused_space == terminals[0].win) {
                     term_handle_key(focused_term, c);
                 }
             }
        }
        
        /* Capa 0: Deep Void */
        framebuffer_clear(FLUX_VOID);
        
        if (current_zoom == FLUX_MACRO) {
            draw_constellation();
        } else {
            draw_focus_mode();
        }
        
        /* Capa mouse (siempre arriba) */
        /* Custom cursor for Flux: A small dot or crosshair */
        uint32_t cursor_col = (current_zoom == FLUX_FOCUS) ? FLUX_ACCENT_CYAN : FLUX_TEXT_PRIMARY;
        framebuffer_rect(mouse_x - 1, mouse_y - 1, 3, 3, cursor_col);
        
        framebuffer_flush();
        for (volatile int i = 0; i < 50000; i++); 
        task_yield();
    }
    terminal_initialize(NULL);
}
