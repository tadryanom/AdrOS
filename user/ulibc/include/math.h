#ifndef ULIBC_MATH_H
#define ULIBC_MATH_H

static inline double fabs(double x) { return x < 0 ? -x : x; }
static inline float fabsf(float x) { return x < 0 ? -x : x; }

#endif
