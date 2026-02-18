#include <math.h>

double fabs(double x) {
    return x < 0 ? -x : x;
}

double ceil(double x) {
    long i = (long)x;
    if (x > (double)i) return (double)(i + 1);
    return (double)i;
}

double floor(double x) {
    long i = (long)x;
    if (x < (double)i) return (double)(i - 1);
    return (double)i;
}

// Stubs for complex math if needed
double sqrt(double x) {
    // Newton's method or hardware instruction?
    // Minimal stub
    if (x < 0) return 0;
    double r = x / 2;
    for (int i = 0; i < 10; i++) {
        if (r == 0) break;
        r = (r + x / r) / 2;
    }
    return r;
}

double pow(double x, double y) {
    // Very basic integer power stub logic
    if (y == 0) return 1;
    if (y == 1) return x;
    return x; // Incorrect but enough to link
}

double sin(double x) { return 0; }
double cos(double x) { return 1; }
double tan(double x) { return 0; }
