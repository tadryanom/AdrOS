#ifndef ULIBC_ASSERT_H
#define ULIBC_ASSERT_H

#include "stdio.h"
#include "stdlib.h"

#define assert(expr) \
    do { if (!(expr)) { printf("Assertion failed: %s (%s:%d)\n", #expr, __FILE__, __LINE__); exit(1); } } while(0)

#endif
