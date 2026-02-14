/*
 * éterOS - Desktop Environment (Flux UI)
 * 
 * Implementación de un entorno de escritorio básico con:
 * - Barra de tareas y Menú Inicio.
 * - Ventana de Terminal interactiva.
 * - Soporte de Mouse y Teclado.
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
#include <shell.h> /* Para reutilizar lógica o definiciones si es necesario */
#include <pmm.h>   /* Para estadísticas de RAM */

/* ========================================================================= */
/* Constantes y Estado Global                                                */
/* ========================================================================= */

static volatile bool desktop_running = true;

/* Ventanas del sistema */
static window_t* win_terminal = NULL;
static window_t* win_sysinfo = NULL;

/* Estado del Mouse */
static int32_t mouse_x = 512;
static int32_t mouse_y = 384;
static bool    mouse_left_btn = false;

/* Arrastre de ventanas */
static window_t* win_drag = NULL;
static int32_t drag_offset_x = 0;
static int32_t drag_offset_y = 0;

/* Estado del Menú Inicio */
static bool menu_open = false;

/* ========================================================================= */
/* Terminal App Logica                                                       */
/* ========================================================================= */

#define TERM_MAX_LINES 15
#define TERM_MAX_COLS  40
#define TERM_BUFFER_SIZE (TERM_MAX_LINES * TERM_MAX_COLS)

static char term_buffer[TERM_MAX_LINES][TERM_MAX_COLS + 1]; /* +1 para null */
static int  term_cursor_x = 0;
static int  term_cursor_y = 0;
static char term_input_buf[64];
static int  term_input_pos = 0;

static void term_clear(void) {
    for (int i = 0; i < TERM_MAX_LINES; i++) {
        memset(term_buffer[i], 0, sizeof(term_buffer[i]));
    }
    term_cursor_x = 0;
    term_cursor_y = 0;
    term_input_pos = 0;
    memset(term_input_buf, 0, sizeof(term_input_buf));
}

/* Output hook para redireccionar stdout del kernel a esta ventana */
static void gui_term_hook(char c);

static void term_putc(char c) {
    if (c == '\n') {
        term_cursor_y++;
        term_cursor_x = 0;
    } else if (c == '\b') {
        if (term_cursor_x > 0) {
            term_cursor_x--;
            term_buffer[term_cursor_y][term_cursor_x] = 0;
        }
    } else if (term_cursor_x < TERM_MAX_COLS - 1) {
        term_buffer[term_cursor_y][term_cursor_x++] = c;
        term_buffer[term_cursor_y][term_cursor_x] = 0;
    }

    /* Scroll simple */
    if (term_cursor_y >= TERM_MAX_LINES) {
        /* Mover lineas arriba */
        for (int i = 0; i < TERM_MAX_LINES - 1; i++) {
            memcpy(term_buffer[i], term_buffer[i+1], TERM_MAX_COLS + 1);
        }
        memset(term_buffer[TERM_MAX_LINES-1], 0, TERM_MAX_COLS + 1);
        term_cursor_y = TERM_MAX_LINES - 1;
    }
}

static void term_print(const char* text) {
    while (*text) {
        term_putc(*text++);
    }
}

/* Hook que será llamado por terminal_putchar/printf del kernel */
static void gui_term_hook(char c) {
    term_putc(c);
}

static void term_execute_cmd(const char* cmd) {
    term_print("\n");
    
    /* Comandos locales de la GUI */
    if (strcmp(cmd, "clear") == 0) {
        term_clear();
        return; /* No imprimir prompt extra si limpiamos */
    }
    
    if (strcmp(cmd, "exit") == 0) {
        if (win_terminal) win_terminal->active = false;
        term_clear();
        return;
    }

    /* Hookear salida del kernel a esta ventana */
    /* Guardamos el hook anterior si quisiéramos restaurarlo, pero aquí asumimos serial/null */
    terminal_set_hook(gui_term_hook);
    
    /* Ejecutar comando real del kernel */
    /* Esto soportará 'help', 'sysinfo', 'mem', 'net', etc. */
    shell_exec((char*)cmd);
    
    /* Restaurar hook (opcional, o dejarlo para logs de fondo?) */
    /* Si lo dejamos, el kernel seguirá escribiendo aquí. */
    /* Mejor quitamos el hook para evitar que logs de sistema (ej: driver e1000)
       ensucien la ventana si no está enfocada o si cerramos la app. */
    terminal_set_hook(NULL);
    
    term_print("\neterOS> ");
}

static void term_handle_key(char c) {
    if (c == '\n') {
        /* Ejecutar */
        term_execute_cmd(term_input_buf);
        memset(term_input_buf, 0, sizeof(term_input_buf));
        term_input_pos = 0;
    } else if (c == '\b') {
        if (term_input_pos > 0) {
            term_input_pos--;
            term_input_buf[term_input_pos] = 0;
            term_putc('\b'); /* Visual backspace */
        }
    } else {
        if (term_input_pos < 60) {
            term_input_buf[term_input_pos++] = c;
            term_input_buf[term_input_pos] = 0;
            term_putc(c); /* Echo local */
        }
    }
}

static void draw_terminal_content(void) {
    wm_fill_rect(win_terminal, (rect_t){0, 0, 340, 260}, UI_COLOR_BLACK);
    
    for (int i = 0; i < TERM_MAX_LINES; i++) {
        if (term_buffer[i][0] != 0) {
            wm_print_at(win_terminal, 5, 5 + (i * 16), term_buffer[i]);
        }
    }
    
    /* Cursor */
    int cursor_px = 5 + (term_cursor_x * 8);
    int cursor_py = 5 + (term_cursor_y * 16);
    /* Parpadeo simulado con el frame count del sistema o simple fijo */
    wm_fill_rect(win_terminal, (rect_t){cursor_px, cursor_py + 12, 8, 2}, UI_COLOR_GREEN);
}

/* ========================================================================= */
/* System Info App                                                           */
/* ========================================================================= */
/* ========================================================================= */
/* System Info App (Real-Time Monitor)                                       */
/* ========================================================================= */
static void draw_progress_bar(window_t* win, int x, int y, int w, int h, float percent, uint32_t color) {
    /* Fondo barra (gris oscuro) */
    rect_t bg = {x, y, w, h};
    wm_fill_rect(win, bg, 0x404040);
    
    /* Progreso */
    int fill_w = (int)((float)w * percent);
    if (fill_w > w) fill_w = w;
    if (fill_w < 0) fill_w = 0;
            
    rect_t fill = {x, y, fill_w, h};
    wm_fill_rect(win, fill, color);
    
    /* Borde */
    // ui_draw_rect... (ya tenemos fill)
}

static void draw_sysinfo_content(void) {
    /* Limpiar fondo para evitar sobre-escritura sucia */
    wm_fill_rect(win_sysinfo, (rect_t){0, 0, 240, 140}, UI_COLOR_DARK);

    wm_print_at(win_sysinfo, 10, 10, "eterOS Genesis - Monitor");
    
    /* 1. CPU Info (Static for now) */
    wm_print_at(win_sysinfo, 10, 30, "CPU: x86_64 (1 Core)");
    
    /* 2. RAM Usage (Dynamic) */
    uint64_t total = pmm_get_total_ram();
    uint64_t free  = pmm_get_free_ram();
    uint64_t used  = total - free;
    
    char buf[64];
    /* Mostrar en MB */
    itoa_s(used / (1024*1024), buf, sizeof(buf), 10);
    char line[64] = "RAM: ";
    strlcpy(line + 5, buf, 64);
    strlcpy(line + strlen(line), " MB / ", 64);
    itoa_s(total / (1024*1024), buf, sizeof(buf), 10);
    strlcpy(line + strlen(line), buf, 64);
    strlcpy(line + strlen(line), " MB", 64);
    
    wm_print_at(win_sysinfo, 10, 50, line);
    
    /* Barra de RAM */
    float usage_pct = (float)used / (float)total;
    uint32_t bar_color = (usage_pct > 0.8f) ? UI_COLOR_RED : UI_COLOR_GREEN;
    draw_progress_bar(win_sysinfo, 10, 65, 180, 10, usage_pct, bar_color);

    /* 3. Uptime (Dynamic) */
    uint32_t uptime = timer_get_uptime_seconds();
    int min = uptime / 60;
    int sec = uptime % 60;
    
    char time_str[32];
    itoa_s(min, buf, sizeof(buf), 10);
    strlcpy(time_str, "Uptime: ", 32);
    strlcpy(time_str + strlen(time_str), buf, 32);
    strlcpy(time_str + strlen(time_str), "m ", 32);
    itoa_s(sec, buf, sizeof(buf), 10);
    strlcpy(time_str + strlen(time_str), buf, 32);
    strlcpy(time_str + strlen(time_str), "s", 32);
    
    wm_print_at(win_sysinfo, 10, 90, time_str);
    
    wm_print_at(win_sysinfo, 10, 110, "Video: 1024x768 (32bpp)");
}

/* ========================================================================= */
/* Lógica de Escritorio                                                      */
/* ========================================================================= */

static void draw_taskbar(void) {
    /* Barra gris abajo */
    framebuffer_rect(0, 748, 1024, 20, 0xC0C0C0); /* Gris claro */
    framebuffer_rect(0, 748, 1024, 1, UI_COLOR_WHITE); /* Borde superior */

    /* Botón Start */
    uint32_t start_color = menu_open ? 0x808080 : 0xC0C0C0;
    framebuffer_rect(2, 750, 60, 16, start_color);
    
    /* Efecto botón 3D simple */
    if (!menu_open) {
        framebuffer_rect(2, 750, 60, 1, UI_COLOR_WHITE);
        framebuffer_rect(2, 750, 1, 16, UI_COLOR_WHITE);
        framebuffer_rect(62, 750, 1, 16, 0x404040);
        framebuffer_rect(2, 766, 60, 1, 0x404040);
    }
    
    /* Texto "Inicio" */
    ui_draw_string(NULL, 10, 754, "Inicio", 0x000000, start_color);
    
    /* Reloj (Simulado) */
    framebuffer_rect(950, 750, 70, 16, 0xA0A0A0);
    ui_draw_string(NULL, 955, 754, "12:00", 0x000000, 0xA0A0A0);
}

/* ========================================================================= */
/* Matrix Rain App (Background Task Demo)                                    */
/* ========================================================================= */
static window_t* win_matrix = NULL;
static volatile bool matrix_running = false;

/* Estado de la lluvia (simple por ahora) */
#define MATRIX_COLS 40
#define MATRIX_ROWS 20
static char matrix_chars[MATRIX_COLS][MATRIX_ROWS];
static int  matrix_drops[MATRIX_COLS]; /* Posición Y de la gota en cada columna */

void task_matrix_rain(void) {
    /* Inicializar estado */
    for (int x = 0; x < MATRIX_COLS; x++) {
        matrix_drops[x] = -(x % 15); /* Desfase inicial */
        for (int y = 0; y < MATRIX_ROWS; y++) {
            matrix_chars[x][y] = ' ';
        }
    }

    while (matrix_running && win_matrix && win_matrix->active) {
        /* Actualizar lógica Matrix */
        for (int x = 0; x < MATRIX_COLS; x++) {
            /* Mover gota */
            matrix_drops[x]++;
            
            /* Si la gota está en pantalla, dejar rastro */
            int head = matrix_drops[x];
            if (head >= 0 && head < MATRIX_ROWS) {
                /* Caracter aleatorio (simple hack) */
                static const char charset[] = "01XZ@#%&";
                /* Random muy simple usando timer */
                char c = charset[(timer_get_uptime_seconds() + head + x) % 8];
                matrix_chars[x][head] = c;
            }
            
            /* Borrar cola */
            int tail = head - 8; /* Longitud rastro */
            if (tail >= 0 && tail < MATRIX_ROWS) {
                matrix_chars[x][tail] = ' ';
            }
            
            /* Reset si cae del todo */
            if (head > MATRIX_ROWS + 5) {
                matrix_drops[x] = -(x % 10) - 2; /* Reiniciar arriba */
            }
        }

        /* Dibujar Frame (Directo a ventana - OJO con concurrencia) */
        /* Idealmente usaríamos mensajes, aquí dibujamos directo */
        /* Fondo negro */
        wm_fill_rect(win_matrix, (rect_t){0, 0, 320, 200}, UI_COLOR_BLACK);
        
        for (int x = 0; x < MATRIX_COLS; x++) {
            for (int y = 0; y < MATRIX_ROWS; y++) {
                char c = matrix_chars[x][y];
                if (c != ' ') {
                    /* Color: Verde brillante para cabeza, oscuro para cola */
                    uint32_t color = UI_COLOR_GREEN;
                    if (y < matrix_drops[x]) color = 0xFF008000; /* Dark Green */
                    if (y == matrix_drops[x]) color = UI_COLOR_WHITE; /* Head */
                    
                    /* Hack dibujo char */
                    char s[2] = {c, 0};
                    wm_print_at(win_matrix, x * 8, y * 10, s /* Color custom no soportado en print_at simple aún */);
                    /* Hack directo: */
                    /* ui_draw_char(NULL, win_matrix->bounds.x + x*8, win_matrix->bounds.y + 20 + y*10, c, color, UI_COLOR_BLACK); */
                    /* Usamos rects para simular pixeles si print_at no soporta color por char */
                    /* Por ahora print_at usa fg_color de ventana. */
                }
            }
        }
        
        /* Dormir un poco (50ms) */
        task_sleep(50);
    }
    
    matrix_running = false;
    win_matrix = NULL;
    task_exit();
}

static void draw_start_menu(void) {
    if (!menu_open) return;
    
    /* Fondo menú (más alto ahora) */
    framebuffer_rect(0, 570, 150, 178, 0xC0C0C0);
    framebuffer_rect(0, 570, 150, 1, UI_COLOR_WHITE);
    framebuffer_rect(150, 570, 1, 178, 0x404040);
    
    /* Items */
    /* 1. Matrix */
    framebuffer_rect(4, 580, 142, 20, 0xC0C0C0); 
    ui_draw_string(NULL, 25, 586, "Matrix Demo", 0x000000, 0xC0C0C0);
    framebuffer_rect(10, 586, 10, 8, UI_COLOR_GREEN);
    
    /* 2. Terminal */
    framebuffer_rect(4, 610, 142, 20, 0x000080); /* Azul highlight */
    ui_draw_string(NULL, 25, 616, "Terminal", UI_COLOR_WHITE, 0x000080);
    framebuffer_rect(10, 616, 10, 8, UI_COLOR_WHITE); /* Icono term */
    
    /* 3. SysInfo */
    framebuffer_rect(4, 640, 142, 20, 0xC0C0C0);
    ui_draw_string(NULL, 25, 646, "System Info", 0x000000, 0xC0C0C0);
    framebuffer_rect(10, 646, 10, 8, UI_COLOR_RED); /* Icono sys */
    
    /* 4. Reboot */
    framebuffer_rect(4, 720, 142, 20, 0xC0C0C0); 
    ui_draw_string(NULL, 25, 726, "Reboot", 0x000000, 0xC0C0C0);
    framebuffer_rect(10, 726, 10, 8, 0x000000); /* Icono off */
}

/* ========================================================================= */
/* Input Handling                                                            */
/* ========================================================================= */

static void handle_mouse_click(void) {
    /* 1. Start Menu Click */
    if (mouse_y >= 748 && mouse_x < 64) {
        menu_open = !menu_open;
        return;
    }
    
    if (menu_open) {
        /* Click en items del menú */
        if (mouse_x < 150 && mouse_y >= 570) {
            
            /* Matrix Click (580-600) */
            if (mouse_y >= 580 && mouse_y <= 600) {
                if (!win_matrix) {
                    win_matrix = wm_create_window(50, 50, 320, 220, "Matrix Rain");
                    win_matrix->bg_color = UI_COLOR_BLACK;
                    win_matrix->fg_color = UI_COLOR_GREEN;
                    
                    /* Lanzar Tarea de Fondo */
                    matrix_running = true;
                    task_create("matrix_rain", task_matrix_rain);
                } else {
                    win_matrix->active = true;
                }
                menu_open = false;
                return;
            }

            /* Terminal Click */
            if (mouse_y >= 610 && mouse_y <= 630) {
                if (!win_terminal) {
                    win_terminal = wm_create_window(100, 100, 340, 260, "Terminal");
                    /* Forzar fondo negro para que el texto coincida */
                    win_terminal->bg_color = UI_COLOR_BLACK; 
                    term_clear();
                    term_print("eterOS Genesis Shell\n");
                    term_print("Type 'help' for commands\n\n");
                    term_print("eterOS> ");
                } else {
                    win_terminal->active = true;
                    /* Bring to front (hack: redraw last) */
                }
                menu_open = false;
                return;
            }
            
            /* SysInfo Click */
            if (mouse_y >= 640 && mouse_y <= 660) {
                if (!win_sysinfo) {
                    /* Aumentado tamaño para info extra */
                    win_sysinfo = wm_create_window(200, 150, 260, 160, "System Info");
                } else {
                    win_sysinfo->active = true;
                }
                menu_open = false;
                return;
            }

            /* Reboot Click */
            if (mouse_y >= 720 && mouse_y <= 740) {
                desktop_running = false; /* Exit to shell -> then reboot */
                /* O llamar sys_reboot() */
            }
        } else {
            /* Click fuera cierra menú */
            menu_open = false;
        }
    }

    /* 2. Window Interaction (Close / Drag / Focus) */
    /* Check Windows (Reverse Order: Front to Back) */
    
    /* TODO: Implement generic window list iteration */
    /* For now, checking hardcoded windows, Terminal has priority if active/front */
    
    bool handled = false;
    
    /* Helper macro to check window hit */
    #define CHECK_WINDOW_HIT(win) \
        if (win && win->active) { \
            if (mouse_x >= win->bounds.x && mouse_x <= win->bounds.x + win->bounds.w && \
                mouse_y >= win->bounds.y && mouse_y <= win->bounds.y + 20) { \
                \
                /* Check Close Button [X] (Rightmost 16px) */ \
                if (mouse_x >= win->bounds.x + win->bounds.w - 18) { \
                    win->active = false; \
                    handled = true; \
                } else { \
                    /* Drag Start */ \
                    win_drag = win; \
                    drag_offset_x = mouse_x - win->bounds.x; \
                    drag_offset_y = mouse_y - win->bounds.y; \
                    /* Focus (Simple Hack: Just set touched flag, real Z-order needs list) */ \
                    handled = true; \
                } \
            } \
        }

    /* Check Terminal */
    CHECK_WINDOW_HIT(win_terminal);
    if (handled) return;

    /* Check SysInfo */
    CHECK_WINDOW_HIT(win_sysinfo);
    if (handled) return;
}

static void on_mouse_event(int8_t dx, int8_t dy, uint8_t buttons) {
    mouse_x += dx;
    mouse_y += dy;
    
    /* Clamp */
    if (mouse_x < 0) mouse_x = 0;
    if (mouse_y < 0) mouse_y = 0;
    if (mouse_x >= 1024) mouse_x = 1023;
    if (mouse_y >= 768) mouse_y = 767;

    bool left_btn = (buttons & 1);
    
    if (left_btn && !mouse_left_btn) {
        /* Mouse Down Event */
        handle_mouse_click();
    } else if (!left_btn && mouse_left_btn) {
        /* Mouse Up Event */
        win_drag = NULL;
    }
    
    mouse_left_btn = left_btn;

    /* Dragging Logic */
    if (win_drag) {
        win_drag->bounds.x = mouse_x - drag_offset_x;
        win_drag->bounds.y = mouse_y - drag_offset_y;
    }
}

/* ========================================================================= */
/* Main Desktop Loop                                                         */
/* ========================================================================= */

void gui_demo_run(void) {
    /* Inicializar sistema gráfico */
    wm_init();
    framebuffer_enable_double_buffer();
    mouse_set_callback(on_mouse_event);
    
    desktop_running = true;
    
    /* Loop principal */
    while (desktop_running) {
        
        /* 1. INPUT (Keyboard) */
        /* TODO: Buffer de teclado en el Kernel para no perder teclas */
        if (keyboard_has_input()) {
            char c = keyboard_getchar();
            
            if (c == 27) { /* ESC: Abrir/Cerrar menú */
                menu_open = !menu_open;
            } else {
                /* Enviar keystroke a ventana activa (focus) */
                /* Por ahora asumimos Terminal siempre tiene foco si existe */
                if (win_terminal && win_terminal->active) {
                    term_handle_key(c);
                }
            }
        }
        
        /* 2. RENDER */
        /* A. Desktop Background */
        framebuffer_clear(0x008080); /* Windows 95 Teal */
        
        /* B. Windows */
        if (win_sysinfo && win_sysinfo->active) {
            draw_sysinfo_content();
            wm_draw_window(win_sysinfo);
        }
        
        if (win_terminal && win_terminal->active) {
            draw_terminal_content();
            wm_draw_window(win_terminal);
        }
        
        /* C. UI Elements (Taskbar, Menus) */
        draw_taskbar();
        draw_start_menu();
        
        /* D. Mouse Cursor */
        framebuffer_rect(mouse_x, mouse_y, 0, 0, 0); /* Dummy call? No */
        /* Dibujar flecha simple */
        framebuffer_rect(mouse_x, mouse_y, 2, 12, UI_COLOR_WHITE);
        framebuffer_rect(mouse_x, mouse_y, 12, 2, UI_COLOR_WHITE);
        framebuffer_rect(mouse_x+1, mouse_y+1, 10, 1, UI_COLOR_BLACK);
        framebuffer_rect(mouse_x+1, mouse_y+1, 1, 10, UI_COLOR_BLACK);
        
        /* 3. FLUSH */
        framebuffer_flush();
        
        /* 4. SYNC */
        /* Simple delay para ~60fps */
        for (volatile int i = 0; i < 50000; i++); 
        task_yield();
    }
    
    /* Al salir, limpiar pantalla y volver a modo texto */
    terminal_initialize(NULL);
}


