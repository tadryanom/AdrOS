#ifndef ULIBC_LINUX_FUTEX_H
#define ULIBC_LINUX_FUTEX_H

#include <stdint.h>

#define FUTEX_WAIT 0
#define FUTEX_WAKE 1

int futex(uint32_t* uaddr, int op, uint32_t val);

#endif
