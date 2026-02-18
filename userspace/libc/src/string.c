/**
 * éterOS Mini-LibC - String Functions
 * musl-compatible memory and string operations
 */

#include <string.h>

void *memcpy(void *dest, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;

    /* Fast 8-byte copy path */
    while (n >= 8) {
        *(uint64_t *)d = *(const uint64_t *)s;
        d += 8; s += 8; n -= 8;
    }
    while (n--) *d++ = *s++;
    return dest;
}

void *memset(void *s, int c, size_t n) {
    uint8_t *p = (uint8_t *)s;
    uint8_t val = (uint8_t)c;

    /* Fast 8-byte fill path */
    if (n >= 8) {
        uint64_t wide = val;
        wide |= wide << 8;
        wide |= wide << 16;
        wide |= wide << 32;
        while (n >= 8) {
            *(uint64_t *)p = wide;
            p += 8; n -= 8;
        }
    }
    while (n--) *p++ = val;
    return s;
}

void *memmove(void *dest, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;

    if (d < s) {
        while (n--) *d++ = *s++;
    } else if (d > s) {
        d += n; s += n;
        while (n--) *--d = *--s;
    }
    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const uint8_t *a = (const uint8_t *)s1;
    const uint8_t *b = (const uint8_t *)s2;
    while (n--) {
        if (*a != *b) return (int)*a - (int)*b;
        a++; b++;
    }
    return 0;
}

size_t strlen(const char *s) {
    const char *p = s;
    while (*p) p++;
    return (size_t)(p - s);
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && *s1 == *s2) { s1++; s2++; }
    return (int)(unsigned char)*s1 - (int)(unsigned char)*s2;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    while (n && *s1 && *s1 == *s2) { s1++; s2++; n--; }
    if (n == 0) return 0;
    return (int)(unsigned char)*s1 - (int)(unsigned char)*s2;
}

char *strcpy(char *dest, const char *src) {
    char *d = dest;
    while ((*d++ = *src++));
    return dest;
}

char *strncpy(char *dest, const char *src, size_t n) {
    char *d = dest;
    while (n && (*d++ = *src++)) n--;
    while (n--) *d++ = '\0';
    return dest;
}

char *strcat(char *dest, const char *src) {
    char *d = dest;
    while (*d) d++;
    while ((*d++ = *src++));
    return dest;
}

char *strchr(const char *s, int c) {
    while (*s) {
        if (*s == (char)c) return (char *)s;
        s++;
    }
    return (c == 0) ? (char *)s : (void*)0;
}

char *strrchr(const char *s, int c) {
    const char *last = (void*)0;
    while (*s) {
        if (*s == (char)c) last = s;
        s++;
    }
    if (c == 0) return (char *)s;
    return (char *)last;
}

char *strstr(const char *haystack, const char *needle) {
    if (!*needle) return (char *)haystack;
    size_t nlen = strlen(needle);
    while (*haystack) {
        if (strncmp(haystack, needle, nlen) == 0)
            return (char *)haystack;
        haystack++;
    }
    return (void*)0;
}

static int tolower(int c) {
    if (c >= 'A' && c <= 'Z') return c + ('a' - 'A');
    return c;
}

int strcasecmp(const char *s1, const char *s2) {
    const unsigned char *p1 = (const unsigned char *)s1;
    const unsigned char *p2 = (const unsigned char *)s2;
    int result;
    if (p1 == p2) return 0;
    while ((result = tolower(*p1) - tolower(*p2)) == 0) {
        if (*p1++ == '\0') return 0;
        p2++;
    }
    return result;
}

int strncasecmp(const char *s1, const char *s2, size_t n) {
    const unsigned char *p1 = (const unsigned char *)s1;
    const unsigned char *p2 = (const unsigned char *)s2;
    int result;
    if (p1 == p2) return 0;
    while (n > 0) {
        result = tolower(*p1) - tolower(*p2);
        if (result != 0) return result;
        if (*p1++ == '\0') return 0;
        p2++;
        n--;
    }
    return 0;
}

#include <stdlib.h>

char *strdup(const char *s) {
    size_t len = strlen(s) + 1;
    char *new_str = malloc(len);
    if (new_str) {
        memcpy(new_str, s, len);
    }
    return new_str;
}
