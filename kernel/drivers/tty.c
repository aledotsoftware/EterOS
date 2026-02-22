#include <fs/vfs.h>
#include <keyboard.h>
#include <vga.h>
#include <serial.h>
#include <mm.h>
#include <string.h>
#include <types.h>
#include <termios.h>
#include <sys/ioctl.h>

/* TTY Node Functions */

static struct termios tty_std_termios = {
    .c_iflag = 0,
    .c_oflag = 0,
    .c_cflag = 0,
    .c_lflag = ECHO | ICANON,
    .c_line = 0,
    .c_cc = {
        [VMIN] = 1,
        [VTIME] = 0,
        [VERASE] = '\b',
    }
};

static uint32_t tty_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)node; (void)offset;

    struct termios *t = (struct termios*)(void*)node->ptr;
    if (!t) t = &tty_std_termios; /* Fallback */

    if (size == 0) return 0;

    /* Read from keyboard buffer */
    /* This blocks until input is available */
    uint32_t i = 0;
    while (i < size) {
        /* TODO: Handle non-blocking if O_NONBLOCK is set */
        char c = keyboard_getchar();

        /* Canonical Mode (Line Buffering) */
        if (t->c_lflag & ICANON) {
             /* Handle Backspace */
            if (c == '\b') {
                if (i > 0) {
                    i--;
                    /* Erase from screen if ECHO is set */
                    if (t->c_lflag & ECHO) {
                        terminal_putchar('\b');
                        terminal_putchar(' ');
                        terminal_putchar('\b');
                        serial_putchar('\b');
                        serial_putchar(' ');
                        serial_putchar('\b');
                    }
                }
                continue;
            }

            /* Echo */
            if (t->c_lflag & ECHO) {
                terminal_putchar(c);
                serial_putchar(c);
            }

            buffer[i++] = c;
            if (c == '\n' || c == '\r') {
                /* Normalize newline */
                if (c == '\r') buffer[i-1] = '\n';
                return i;
            }
        } else {
            /* Raw Mode */
            buffer[i++] = c;

            /* Echo */
            if (t->c_lflag & ECHO) {
                terminal_putchar(c);
                serial_putchar(c);
            }

            /* Check VMIN */
            if (i >= t->c_cc[VMIN]) {
                return i;
            }
        }
    }
    return i;
}

static uint32_t tty_write(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)node; (void)offset;

    for (uint32_t i = 0; i < size; i++) {
        terminal_putchar(buffer[i]);
        serial_putchar(buffer[i]);
    }
    return size;
}

static int tty_ioctl(fs_node_t *node, int request, void *arg) {
    struct termios *t = (struct termios*)(void*)node->ptr;
    if (!t) return -1;

    switch (request) {
        case TCGETS:
            if (!arg) return -1;
            memcpy(arg, t, sizeof(struct termios));
            return 0;
        case TCSETS:
            if (!arg) return -1;
            memcpy(t, arg, sizeof(struct termios));
            return 0;
        default:
            return -1;
    }
}

fs_node_t* tty_create_node(void) {
    fs_node_t* tty = (fs_node_t*)kmalloc(sizeof(fs_node_t));
    if (!tty) return NULL;

    memset(tty, 0, sizeof(fs_node_t));
    strlcpy(tty->name, "tty", 128);
    tty->flags = FS_CHARDEVICE;
    tty->inode = 0; /* Device ID? */
    tty->length = 0;
    tty->read = tty_read;
    tty->write = tty_write;
    tty->open = NULL;
    tty->close = NULL;
    tty->ioctl = tty_ioctl;
    tty->ref_count = 1;
    tty->lock = 0;
    tty->ptr = (struct fs_node *)(void*)&tty_std_termios; /* Store tty state */

    return tty;
}
