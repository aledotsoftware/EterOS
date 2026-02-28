#include <fs/vfs.h>
#include <keyboard.h>
#include <vga.h>
#include <serial.h>
#include <mm.h>
#include <string.h>
#include <types.h>
#include <fcntl.h>
#include <errno.h>
#include <ioctl.h>
#include <termios.h>
#include <stdbool.h>

/* TTY Node Functions */

static struct termios tty_state;
static bool tty_state_initialized = false;

static ssize_t tty_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)node; (void)offset;

    if (size == 0) return 0;

    /* Read from keyboard buffer */
    /* This blocks until input is available */
    uint32_t i = 0;
    while (i < size) {
        if ((node->flags & O_NONBLOCK) && !keyboard_has_input()) {
            if (i == 0) return -EAGAIN;
            return i;
        }
        char c = keyboard_getchar();

        /* Echo to screen and serial if ECHO is set */
        if (tty_state.c_lflag & ECHO) {
            terminal_putchar(c);
            serial_putchar(c);
        }

        if (tty_state.c_lflag & ICANON) {
            /* Canonical mode: line editing and line buffering */
            /* Handle Backspace */
            if (c == '\b') {
                if (i > 0) {
                    i--;
                    if (tty_state.c_lflag & ECHO) {
                        /* Erase from screen: Backspace + Space + Backspace */
                        terminal_putchar(' ');
                        terminal_putchar('\b');
                        /* Serial backspace */
                        serial_putchar(' ');
                        serial_putchar('\b');
                    }
                }
                continue;
            }

            buffer[i++] = c;
            if (c == '\n' || c == '\r') {
                /* Normalize newline */
                if (c == '\r') buffer[i-1] = '\n';
                return i;
            }
        } else {
            /* Raw mode: immediately place in buffer and return 1 byte (VMIN=1 style) */
            buffer[i++] = c;
            return i;
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
    (void)node;

    if (!arg) return -EINVAL;

    if (request == TCGETS) {
        memcpy(arg, &tty_state, sizeof(struct termios));
        return 0;
    } else if (request == TCSETS) {
        memcpy(&tty_state, arg, sizeof(struct termios));
        return 0;
    }

    return -EINVAL;
}

fs_node_t* tty_create_node(void) {
    if (!tty_state_initialized) {
        memset(&tty_state, 0, sizeof(struct termios));
        tty_state.c_lflag = ECHO | ICANON | ISIG;
        tty_state_initialized = true;
    }

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

    return tty;
}
