#ifndef ULIBC_SYS_TIME_H
#define ULIBC_SYS_TIME_H

#include <stdint.h>

struct timeval {
    uint32_t tv_sec;
    uint32_t tv_usec;
};

struct itimerval {
    struct timeval it_interval;  /* timer interval (reload value) */
    struct timeval it_value;     /* current value */
};

#define ITIMER_REAL    0
#define ITIMER_VIRTUAL 1
#define ITIMER_PROF    2

int getitimer(int which, struct itimerval *curr_value);
int setitimer(int which, const struct itimerval *new_value,
              struct itimerval *old_value);

#endif
