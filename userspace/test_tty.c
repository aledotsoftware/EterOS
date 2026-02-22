#include <sys/ioctl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

typedef unsigned int tcflag_t;
typedef unsigned char cc_t;

#define NCCS 32

struct termios {
    tcflag_t c_iflag;
    tcflag_t c_oflag;
    tcflag_t c_cflag;
    tcflag_t c_lflag;
    cc_t c_line;
    cc_t c_cc[NCCS];
};

#define ECHO    0000010
#define ICANON  0000002

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    struct termios t;

    printf("Testing TTY ioctl...\n");

    /* Assume fd 0 is TTY */
    if (ioctl(0, TCGETS, &t) < 0) {
        printf("TCGETS failed on fd 0\n");
        return 1;
    }

    printf("Current c_lflag: %x\n", t.c_lflag);

    if ((t.c_lflag & (ECHO | ICANON)) == (ECHO | ICANON)) {
        printf("ECHO and ICANON are set (Correct default)\n");
    } else {
        printf("Defaults are wrong: %x\n", t.c_lflag);
    }

    /* Toggle ECHO */
    t.c_lflag &= ~ECHO;
    if (ioctl(0, TCSETS, &t) < 0) {
        printf("TCSETS failed\n");
        return 1;
    }

    /* Verify */
    memset(&t, 0, sizeof(t));
    if (ioctl(0, TCGETS, &t) < 0) {
        printf("TCGETS failed verify\n");
        return 1;
    }

    if (!(t.c_lflag & ECHO)) {
        printf("ECHO successfully disabled\n");
    } else {
        printf("Failed to disable ECHO: %x\n", t.c_lflag);
    }

    /* Restore ECHO */
    t.c_lflag |= ECHO;
    ioctl(0, TCSETS, &t);

    return 0;
}
