#ifndef ULIBC_SYS_TIMES_H
#define ULIBC_SYS_TIMES_H

#include <stdint.h>

struct tms {
    uint32_t tms_utime;
    uint32_t tms_stime;
    uint32_t tms_cutime;
    uint32_t tms_cstime;
};

uint32_t times(struct tms* buf);

#endif
