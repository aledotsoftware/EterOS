/**
 * =============================================================================
 * éterOS - SantiTravel v1.0
 * Copyright (c) 2026 Tudex Networks. All rights reserved.
 * =============================================================================
 *
 * 🌍 Primera aplicación nativa del ecosistema éterOS.
 *
 * Registro de viajes y aventuras con Santi.
 * Corre directamente sobre el kernel en modo VGA texto (80x25).
 *
 * Características:
 *   - Animación ASCII de avión despegando
 *   - Lista de destinos visitados (con colores)
 *   - Lista de destinos por visitar
 *   - Estadísticas de viaje
 *
 * =============================================================================
 */

#include "../include/santitravel.h"
#include "../include/vga.h"
#include "../include/keyboard.h"
#include "../include/string.h"
#include "../include/io.h"

/* ========================================================================= */
/* Delay simple (busy-wait calibrado por I/O)                                */
/* ========================================================================= */
static void delay_ms(uint32_t ms) {
    /* ~1ms por iteración usando I/O port read (cada inb ~1µs) */
    for (uint32_t i = 0; i < ms; i++) {
        for (uint32_t j = 0; j < 1000; j++) {
            inb(0x80);  /* Port 0x80 read como delay */
        }
    }
}

/* ========================================================================= */
/* Datos de destinos                                                         */
/* ========================================================================= */

typedef struct {
    const char* city;
    const char* country;
    const char* flag;       /* Emoji/símbolo corto */
    uint8_t     visited;    /* 1 = visitado, 0 = pendiente */
} destination_t;

static const destination_t destinations[] = {
    /* ===== VISITADOS ===== */
    { "Posadas",          "Argentina",  "[MIS]", 1 },
    { "Cataratas Iguazu", "Argentina",  "[IGU]", 1 },
    { "Ciudad del Este",  "Paraguay",   "[PY ]", 1 },
    { "Foz de Iguazu",    "Brasil",     "[BR ]", 1 },
    { "Buenos Aires",     "Argentina",  "[BUE]", 1 },
    { "Ushuaia",          "Argentina",  "[USH]", 1 },
    { "Tolhuin",          "Argentina",  "[TDF]", 1 },
    { "Rio Grande",       "Argentina",  "[TDF]", 1 },
    { "Mendoza",          "Argentina",  "[MDZ]", 1 },
    { "Santiago",         "Chile",      "[SCL]", 1 },
    { "Vina del Mar",     "Chile",      "[VNA]", 1 },

    /* ===== POR VISITAR ===== */
    { "Bariloche",        "Argentina",  "[BRC]", 0 },
    { "El Calafate",      "Argentina",  "[FTE]", 0 },
    { "Salta",            "Argentina",  "[SLA]", 0 },
    { "Rio de Janeiro",   "Brasil",     "[GIG]", 0 },
    { "Cusco",            "Peru",       "[CUZ]", 0 },
    { "Cancun",           "Mexico",     "[CUN]", 0 },
    { "Bogota",           "Colombia",   "[BOG]", 0 },
    { "Cartagena",        "Colombia",   "[CTG]", 0 },
    { "Lima",             "Peru",       "[LIM]", 0 },
    { "Montevideo",       "Uruguay",    "[MVD]", 0 },
    { "Nueva York",       "EEUU",       "[JFK]", 0 },
    { "Tokyo",            "Japon",      "[NRT]", 0 },
    { "Barcelona",        "Espana",     "[BCN]", 0 },
    { "Roma",             "Italia",     "[FCO]", 0 },
};

#define NUM_DESTINATIONS  (sizeof(destinations) / sizeof(destinations[0]))

/* ========================================================================= */
/* Animación del avión ASCII                                                 */
/* ========================================================================= */

static void draw_plane_frame_0(void) {
    terminal_write_colored("                                                  \n", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    terminal_write_colored("                       |                          \n", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    terminal_write_colored("                  _____|_____                     \n", VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    terminal_write_colored("                 /     |     \\                   \n", VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    terminal_write_colored("            ====", VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    terminal_write_colored("| SANTI |", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    terminal_write_colored("====>>>              \n", VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    terminal_write_colored("                 \\_________/                     \n", VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    terminal_write_colored("               ___|_________|___                  \n", VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    terminal_write_colored("  ~~~~~~~~~~~~", VGA_COLOR_CYAN, VGA_COLOR_BLACK);
    terminal_write_colored("|___|___|___|___|", VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    terminal_write_colored("~~~~~~~~~~~~~~\n", VGA_COLOR_CYAN, VGA_COLOR_BLACK);
}

static void draw_plane_frame_1(void) {
    terminal_write_colored("                                                  \n", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    terminal_write_colored("                        /                         \n", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    terminal_write_colored("                  _____|_____                     \n", VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    terminal_write_colored("                 /     |     \\                   \n", VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    terminal_write_colored("            ====", VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    terminal_write_colored("| SANTI |", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    terminal_write_colored("====>>>              \n", VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    terminal_write_colored("                 \\_________/                     \n", VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    terminal_write_colored("                   |     |                        \n", VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    terminal_write_colored("  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n", VGA_COLOR_CYAN, VGA_COLOR_BLACK);
}

static void draw_plane_frame_2(void) {
    terminal_write_colored("                                                  \n", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    terminal_write_colored("                                                  \n", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    terminal_write_colored("                  ___________                     \n", VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    terminal_write_colored("                 /     |     \\                   \n", VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    terminal_write_colored("            ====", VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    terminal_write_colored("|TRAVEL!|", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_write_colored("====>>>              \n", VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    terminal_write_colored("                 \\_________/                     \n", VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    terminal_write_colored("                                                  \n", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    terminal_write_colored("  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n", VGA_COLOR_BLUE, VGA_COLOR_BLACK);
}

static void draw_plane_frame_3(void) {
    terminal_write_colored("              *         .    *                    \n", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    terminal_write_colored("          .        *             .                \n", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    terminal_write_colored("                  ___________       *             \n", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    terminal_write_colored("                 /     |     \\                   \n", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    terminal_write_colored("            ====", VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    terminal_write_colored("|TRAVEL!|", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_write_colored("====>>>   ", VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    terminal_write_colored("VAMOS!\n", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    terminal_write_colored("                 \\_________/                     \n", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    terminal_write_colored("      .                             *             \n", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    terminal_write_colored("  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n", VGA_COLOR_BLUE, VGA_COLOR_BLACK);
}

static void run_airplane_animation(void) {
    /* Frame 0: En tierra */
    terminal_initialize();
    terminal_write_string("\n");
    draw_plane_frame_0();
    delay_ms(700);

    /* Frame 1: Despegando */
    terminal_initialize();
    terminal_write_string("\n");
    draw_plane_frame_1();
    delay_ms(600);

    /* Frame 2: En el aire */
    terminal_initialize();
    terminal_write_string("\n");
    draw_plane_frame_2();
    delay_ms(600);

    /* Frame 3: Volando */
    terminal_initialize();
    terminal_write_string("\n");
    draw_plane_frame_3();
    delay_ms(900);
}

/* ========================================================================= */
/* Pantalla principal                                                        */
/* ========================================================================= */

static void show_destinations(void) {
    terminal_initialize();

    /* ---- Título ---- */
    terminal_write_colored(
        "  ___           _   _ _____                   _  \n",
        VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    terminal_write_colored(
        " / __| __ _ _ _| |_(_)_   _| _ __ ___ _____| | \n",
        VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    terminal_write_colored(
        " \\__ \\/ _` | ' \\  _| | | || '_/ _` \\ V / -_) | \n",
        VGA_COLOR_CYAN, VGA_COLOR_BLACK);
    terminal_write_colored(
        " |___/\\__,_|_||_\\__|_| |_||_| \\__,_|\\_/\\___|_| \n",
        VGA_COLOR_BLUE, VGA_COLOR_BLACK);

    terminal_write_colored(
        "  ========= Aventuras con Santi =========       \n",
        VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);

    /* ---- Estadísticas ---- */
    uint32_t visited = 0, pending = 0;
    for (size_t i = 0; i < NUM_DESTINATIONS; i++) {
        if (destinations[i].visited) visited++;
        else pending++;
    }

    terminal_write_colored("  Visitados: ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    char buf[8];
    itoa_s(visited, buf, sizeof(buf), 10);
    terminal_write_colored(buf, VGA_COLOR_WHITE, VGA_COLOR_BLACK);

    terminal_write_colored("  |  Por ir: ", VGA_COLOR_LIGHT_MAGENTA, VGA_COLOR_BLACK);
    itoa_s(pending, buf, sizeof(buf), 10);
    terminal_write_colored(buf, VGA_COLOR_WHITE, VGA_COLOR_BLACK);

    terminal_write_colored("  |  Total: ", VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    itoa_s(visited + pending, buf, sizeof(buf), 10);
    terminal_write_colored(buf, VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    terminal_write_string("\n\n");

    /* ---- Destinos visitados ---- */
    terminal_write_colored("  [+] VISITADOS:\n", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);

    for (size_t i = 0; i < NUM_DESTINATIONS; i++) {
        if (!destinations[i].visited) continue;

        terminal_write_colored("   * ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        terminal_write_colored(destinations[i].flag, VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
        terminal_write_string(" ");
        terminal_write_colored(destinations[i].city, VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        terminal_write_colored(", ", VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
        terminal_write_colored(destinations[i].country, VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        terminal_write_string("\n");
    }

    terminal_write_string("\n");

    /* ---- Destinos por visitar ---- */
    terminal_write_colored("  [ ] POR VISITAR:\n", VGA_COLOR_LIGHT_MAGENTA, VGA_COLOR_BLACK);

    for (size_t i = 0; i < NUM_DESTINATIONS; i++) {
        if (destinations[i].visited) continue;

        terminal_write_colored("   o ", VGA_COLOR_LIGHT_MAGENTA, VGA_COLOR_BLACK);
        terminal_write_colored(destinations[i].flag, VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
        terminal_write_string(" ");
        terminal_write_string(destinations[i].city);
        terminal_write_colored(", ", VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
        terminal_write_colored(destinations[i].country, VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
        terminal_write_string("\n");
    }

    /* ---- Footer ---- */
    terminal_write_string("\n");
    terminal_write_colored("  (c) 2026 Tudex Networks", VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    terminal_write_colored("  |  Presiona cualquier tecla...\n", VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
}

/* ========================================================================= */
/* Entry Point                                                               */
/* ========================================================================= */

void santitravel_run(void) {
    /* Animación de despegue */
    run_airplane_animation();

    /* Mostrar destinos */
    show_destinations();

    /* Esperar tecla para volver al shell */
    keyboard_getchar();
}
