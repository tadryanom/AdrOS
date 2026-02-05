#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

void syscall_init(void);

enum {
    SYSCALL_WRITE = 1,
};

#endif
