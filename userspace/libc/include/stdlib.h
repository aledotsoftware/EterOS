#ifndef _STDLIB_H
#define _STDLIB_H

#include <stdint.h>
#include <stddef.h>

/* Process termination */
void exit(int status) __attribute__((noreturn));

/* Memory allocation */
void *malloc(size_t size);
void  free(void *ptr);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);

int  abs(int j);
long labs(long j);

/* String conversion */
int   atoi(const char *nptr);
long  atol(const char *nptr);
long  strtol(const char *nptr, char **endptr, int base);
double strtod(const char *nptr, char **endptr);
double atof(const char *nptr);

/* Pseudo-random */
#define RAND_MAX 0x7fffffff
int   rand(void);
void  srand(unsigned int seed);

/* Environment */
void  abort(void) __attribute__((noreturn));

/* Misc */
/* NULL is defined in stddef.h */

#endif /* _STDLIB_H */
