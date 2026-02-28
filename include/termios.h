#ifndef ETEROS_TERMIOS_H
#define ETEROS_TERMIOS_H

#include "types.h"

/* Type definitions for termios */
typedef unsigned int tcflag_t;
typedef unsigned char cc_t;

/* Constants */
#define NCCS 32

/* c_lflag bits */
#define ISIG   0000001
#define ICANON 0000002
#define ECHO   0000010

/* Struct termios (Standard Layout) */
struct termios {
    tcflag_t c_iflag;      /* input mode flags */
    tcflag_t c_oflag;      /* output mode flags */
    tcflag_t c_cflag;      /* control mode flags */
    tcflag_t c_lflag;      /* local mode flags */
    cc_t c_line;           /* line discipline */
    cc_t c_cc[NCCS];       /* control characters */
    unsigned int c_ispeed; /* input speed */
    unsigned int c_ospeed; /* output speed */
};

#endif /* ETEROS_TERMIOS_H */
