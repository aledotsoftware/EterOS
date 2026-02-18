#ifndef _STDIO_H
#define _STDIO_H

#include <stdint.h>
#include <stddef.h>

/* Very basic printf support */
int printf(const char *format, ...);
#include <stdarg.h>

/* File access modes */
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

typedef struct {
    int fd;
    int error;
    int eof;
    /* Buffer could be added here later */
} FILE;

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

/* File access */
FILE *fopen(const char *pathname, const char *mode);
int fclose(FILE *stream);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
int fseek(FILE *stream, long offset, int whence);
long ftell(FILE *stream);
void rewind(FILE *stream);
int fflush(FILE *stream);

/* Formatted output */
int printf(const char *format, ...);
int fprintf(FILE *stream, const char *format, ...);
int sprintf(char *str, const char *format, ...);
int snprintf(char *str, size_t size, const char *format, ...);
int vfprintf(FILE *stream, const char *format, va_list ap);
int vsnprintf(char *str, size_t size, const char *format, va_list ap);
int dprintf(int fd, const char *format, ...);

#endif /* _STDIO_H */
