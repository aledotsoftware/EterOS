#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* Standard Streams */
static FILE _stdin  = { 0, 0, 0 };
static FILE _stdout = { 1, 0, 0 };
static FILE _stderr = { 2, 0, 0 };

FILE *stdin  = &_stdin;
FILE *stdout = &_stdout;
FILE *stderr = &_stderr;

/* ========================================================================= */
/* File Access                                                               */
/* ========================================================================= */

FILE *fopen(const char *pathname, const char *mode) {
    int flags = 0;
    /* Parsing simple modes: r, w, r+, w+ */
    if (strchr(mode, 'r')) {
        if (strchr(mode, '+')) flags = 2; // O_RDWR (placeholder value)
        else flags = 0; // O_RDONLY
    } else if (strchr(mode, 'w')) {
        flags = 1; // O_WRONLY
        /* TODO: Add O_CREAT | O_TRUNC logic if supporting write */
    }

    int fd = open(pathname, flags);
    if (fd < 0) return NULL;

    FILE *f = (FILE *)malloc(sizeof(FILE));
    if (!f) {
        close(fd);
        return NULL;
    }

    f->fd = fd;
    f->error = 0;
    f->eof = 0;
    return f;
}

int fclose(FILE *stream) {
    if (!stream) return -1;
    int ret = close(stream->fd);
    if (stream != stdin && stream != stdout && stream != stderr) {
        free(stream);
    }
    return ret;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    if (!stream || !ptr) return 0;
    size_t total_bytes = size * nmemb;
    ssize_t ret = read(stream->fd, ptr, total_bytes);
    
    if (ret < 0) {
        stream->error = 1;
        return 0;
    }
    if (ret == 0) {
        stream->eof = 1;
        return 0;
    }
    return (size_t)ret / size;
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream) {
    if (!stream || !ptr) return 0;
    size_t total_bytes = size * nmemb;
    ssize_t ret = write(stream->fd, ptr, total_bytes);
    
    if (ret < 0) {
        stream->error = 1;
        return 0;
    }
    return (size_t)ret / size;
}

int fseek(FILE *stream, long offset, int whence) {
    if (!stream) return -1;
    stream->eof = 0;
    int64_t ret = lseek(stream->fd, offset, whence);
    return (ret < 0) ? -1 : 0;
}

long ftell(FILE *stream) {
    if (!stream) return -1;
    return (long)lseek(stream->fd, 0, SEEK_CUR);
}

void rewind(FILE *stream) {
    fseek(stream, 0, SEEK_SET);
}

int fflush(FILE *stream) {
    (void)stream;
    return 0; /* No buffering implemented yet */
}

/* ========================================================================= */
/* Formatted Output                                                          */
/* ========================================================================= */

static void putc_fd(int fd, char c) {
    write(fd, &c, 1);
}

static void print_int_fd(int fd, int n, int base) {
    char buf[32];
    int i = 0;
    int sign = 0;
    if (n == 0) { putc_fd(fd, '0'); return; }
    if (n < 0 && base == 10) { sign = 1; n = -n; }
    unsigned int u = (unsigned int)n;
    while (u > 0) {
        int d = u % base;
        buf[i++] = (d < 10) ? ('0' + d) : ('a' + d - 10);
        u /= base;
    }
    if (sign) putc_fd(fd, '-');
    while (i > 0) putc_fd(fd, buf[--i]);
}

static void print_string_fd(int fd, const char *s) {
    if (!s) s = "(null)";
    while (*s) putc_fd(fd, *s++);
}

/* Helper for vsnprintf */
struct snprintf_ctx {
    char *buf;
    size_t size;
    size_t pos;
};

static void putc_buf(struct snprintf_ctx *ctx, char c) {
    if (ctx->pos < ctx->size - 1) {
        ctx->buf[ctx->pos] = c;
    }
    ctx->pos++;
}

static void print_int_buf(struct snprintf_ctx *ctx, int n, int base) {
    char buf[32];
    int i = 0;
    int sign = 0;
    if (n == 0) { putc_buf(ctx, '0'); return; }
    if (n < 0 && base == 10) { sign = 1; n = -n; }
    unsigned int u = (unsigned int)n;
    while (u > 0) {
        int d = u % base;
        buf[i++] = (d < 10) ? ('0' + d) : ('a' + d - 10);
        u /= base;
    }
    if (sign) putc_buf(ctx, '-');
    while (i > 0) putc_buf(ctx, buf[--i]);
}

static void print_string_buf(struct snprintf_ctx *ctx, const char *s) {
    if (!s) s = "(null)";
    while (*s) putc_buf(ctx, *s++);
}

int vsnprintf(char *str, size_t size, const char *format, va_list ap) {
    struct snprintf_ctx ctx = { str, size, 0 };
    const char *p = format;

    while (*p) {
        if (*p == '%') {
            p++;
            switch (*p) {
                case 'd': print_int_buf(&ctx, va_arg(ap, int), 10); break;
                case 'x': print_int_buf(&ctx, va_arg(ap, int), 16); break;
                case 's': print_string_buf(&ctx, va_arg(ap, char*)); break;
                case 'c': putc_buf(&ctx, (char)va_arg(ap, int)); break;
                case '%': putc_buf(&ctx, '%'); break;
                default:  putc_buf(&ctx, '%'); putc_buf(&ctx, *p); break;
            }
        } else {
            putc_buf(&ctx, *p);
        }
        p++;
    }

    if (size > 0) {
        if (ctx.pos < size) str[ctx.pos] = '\0';
        else str[size - 1] = '\0';
    }
    return ctx.pos;
}

int snprintf(char *str, size_t size, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int ret = vsnprintf(str, size, format, ap);
    va_end(ap);
    return ret;
}

int sprintf(char *str, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    /* Safe assumption? 65536? */
    int ret = vsnprintf(str, 65536, format, ap);
    va_end(ap);
    return ret;
}

int vfprintf(FILE *stream, const char *format, va_list ap) {
    /* Hack: print to buffer then write to stream */
    char buf[1024];
    int len = vsnprintf(buf, sizeof(buf), format, ap);
    if (stream) {
        fwrite(buf, 1, (len < (int)sizeof(buf)) ? len : (int)sizeof(buf)-1, stream);
    }
    return len;
}

int fprintf(FILE *stream, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int ret = vfprintf(stream, format, ap);
    va_end(ap);
    return ret;
}

int dprintf(int fd, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    /* Simplified implementation for directly using fd */
    /* ... reusing print_int_fd ... */ 
    const char *p = format;
    int count = 0;
    while (*p) {
        if (*p == '%') {
            p++;
            switch (*p) {
                case 'd': print_int_fd(fd, va_arg(ap, int), 10); break;
                case 'x': print_int_fd(fd, va_arg(ap, int), 16); break;
                case 's': print_string_fd(fd, va_arg(ap, char*)); break;
                case 'c': putc_fd(fd, (char)va_arg(ap, int)); break;
                case '%': putc_fd(fd, '%'); break;
                default:  putc_fd(fd, '%'); putc_fd(fd, *p); break;
            }
        } else {
            putc_fd(fd, *p);
        }
        p++;
    }
    va_end(ap);
    return count;
}

int printf(const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    char buf[1024];
    int ret = vsnprintf(buf, sizeof(buf), format, ap);
    va_end(ap);
    write(1, buf, ret); /* Stdout is 1 */
    return ret;
}
