#ifndef ULIBC_TIME_H
#define ULIBC_TIME_H

#include <stdint.h>

struct timespec {
    uint32_t tv_sec;
    uint32_t tv_nsec;
};

#define CLOCK_REALTIME  0
#define CLOCK_MONOTONIC 1

int nanosleep(const struct timespec* req, struct timespec* rem);
int clock_gettime(int clk_id, struct timespec* tp);

#endif
