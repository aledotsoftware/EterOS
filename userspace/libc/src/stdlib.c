#include <stdlib.h>
#include <ctype.h>

int abs(int j) {
    return (j < 0) ? -j : j;
}

long labs(long j) {
    return (j < 0) ? -j : j;
}

int atoi(const char *nptr) {
    return (int)atol(nptr);
}

long atol(const char *nptr) {
    long result = 0;
    int sign = 1;
    
    while (isspace(*nptr)) nptr++;
    
    if (*nptr == '-') {
        sign = -1;
        nptr++;
    } else if (*nptr == '+') {
        nptr++;
    }
    
    while (isdigit(*nptr)) {
        result = result * 10 + (*nptr - '0');
        nptr++;
    }
    
    return sign * result;
}

long strtol(const char *nptr, char **endptr, int base) {
    long result = 0;
    int sign = 1;
    
    while (isspace(*nptr)) nptr++;
    
    if (*nptr == '-') {
        sign = -1;
        nptr++;
    } else if (*nptr == '+') {
        nptr++;
    }
    
    if (base == 0) {
        if (*nptr == '0') {
            if (tolower(*(nptr+1)) == 'x') {
                base = 16;
                nptr += 2;
            } else {
                base = 8;
            }
        } else {
            base = 10;
        }
    } else if (base == 16) {
        if (*nptr == '0' && tolower(*(nptr+1)) == 'x') {
            nptr += 2;
        }
    }
    
    while (1) {
        int d = -1;
        char c = tolower(*nptr);
        if (isdigit(c)) d = c - '0';
        else if (c >= 'a' && c <= 'z') d = c - 'a' + 10;
        
        if (d < 0 || d >= base) break;
        
        result = result * base + d;
        nptr++;
    }
    
    if (endptr) *endptr = (char *)nptr;
    
    return sign * result;
}

double strtod(const char *nptr, char **endptr) {
    double result = 0.0;
    int sign = 1;
    
    while (isspace(*nptr)) nptr++;
    
    if (*nptr == '-') {
        sign = -1;
        nptr++;
    } else if (*nptr == '+') {
        nptr++;
    }
    
    while (isdigit(*nptr)) {
        result = result * 10.0 + (*nptr - '0');
        nptr++;
    }
    
    if (*nptr == '.') {
        double fraction = 1.0;
        nptr++;
        while (isdigit(*nptr)) {
            fraction /= 10.0;
            result += (*nptr - '0') * fraction;
            nptr++;
        }
    }
    
    if (endptr) *endptr = (char *)nptr;
    
    return sign * result;
}

double atof(const char *nptr) {
    return strtod(nptr, NULL);
}

static unsigned long next = 1;

int rand(void) {
    next = next * 1103515245 + 12345;
    return (unsigned int)(next / 65536) % 32768;
}

void srand(unsigned int seed) {
    next = seed;
}

void abort(void) {
    // Call exit or trap
    exit(134);
}
