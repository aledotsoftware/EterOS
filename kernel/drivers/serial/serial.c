/**
 * éterOS - Implementación del Driver de Puerto Serie
 * 
 * Driver UART 16550 para depuración a través de COM1.
 * Permite enviar mensajes de log al puerto serie, 
 * capturables con QEMU usando -serial stdio.
 */

#include "../include/serial.h"
#include "../include/io.h"

/* ========================================================================= */
/* Registros del UART 16550 (offsets desde la base del puerto)               */
/* ========================================================================= */
#define UART_DATA           0   /* Datos TX/RX */
#define UART_INT_ENABLE     1   /* Habilitación de interrupciones */
#define UART_FIFO_CTRL      2   /* Control de FIFO */
#define UART_LINE_CTRL      3   /* Control de línea */
#define UART_MODEM_CTRL     4   /* Control de modem */
#define UART_LINE_STATUS    5   /* Estado de línea */
#define UART_MODEM_STATUS   6   /* Estado de modem */

/* Bits de estado de línea */
#define LSR_DATA_READY      0x01
#define LSR_TX_EMPTY        0x20

/* ========================================================================= */
/* Implementación                                                            */
/* ========================================================================= */

int serial_init(void) {
    uint16_t port = COM1_PORT;

    /* Deshabilitar todas las interrupciones */
    outb(port + UART_INT_ENABLE, 0x00);

    /* Habilitar DLAB (Divisor Latch Access Bit) para configurar baud rate */
    outb(port + UART_LINE_CTRL, 0x80);

    /* Configurar divisor para 38400 baud (divisor = 3) */
    outb(port + UART_DATA, 0x03);       /* Byte bajo del divisor */
    outb(port + UART_INT_ENABLE, 0x00); /* Byte alto del divisor */

    /* 8 bits, sin paridad, 1 bit de stop (8N1) */
    outb(port + UART_LINE_CTRL, 0x03);

    /* Habilitar FIFO, limpiar buffers, umbral de 14 bytes */
    outb(port + UART_FIFO_CTRL, 0xC7);

    /* Habilitar DTR, RTS, OUT2 (interrupciones habilitadas por hardware) */
    outb(port + UART_MODEM_CTRL, 0x0B);

    /* Modo loopback para test */
    outb(port + UART_MODEM_CTRL, 0x1E);

    /* Enviar byte de prueba */
    outb(port + UART_DATA, 0xAE);

    /* Verificar que recibimos el mismo byte */
    if (inb(port + UART_DATA) != 0xAE) {
        return -1;  /* Puerto serie defectuoso */
    }

    /* Configurar modo normal (sin loopback) */
    outb(port + UART_MODEM_CTRL, 0x0F);

    return 0;
}

static int serial_is_transmit_empty(void) {
    return inb(COM1_PORT + UART_LINE_STATUS) & LSR_TX_EMPTY;
}

void serial_putchar(char c) {
    /* Esperar a que el buffer de transmisión esté vacío */
    while (!serial_is_transmit_empty());
    outb(COM1_PORT + UART_DATA, c);
}

void serial_write_string(const char* str) {
    while (*str) {
        if (*str == '\n') {
            serial_putchar('\r');   /* Agregar CR antes de LF */
        }
        serial_putchar(*str++);
    }
}

int serial_received(void) {
    return inb(COM1_PORT + UART_LINE_STATUS) & LSR_DATA_READY;
}

char serial_read(void) {
    while (!serial_received());
    return (char)inb(COM1_PORT + UART_DATA);
}
