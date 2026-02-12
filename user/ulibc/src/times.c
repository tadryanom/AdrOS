#include "sys/times.h"
#include "syscall.h"

uint32_t times(struct tms* buf) {
    return (uint32_t)_syscall1(SYS_TIMES, (int)buf);
}
