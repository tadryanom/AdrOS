#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

void syscall_init(void);

enum {
    SYSCALL_WRITE = 1,
    SYSCALL_EXIT  = 2,
    SYSCALL_GETPID = 3,

    SYSCALL_OPEN  = 4,
    SYSCALL_READ  = 5,
    SYSCALL_CLOSE = 6,

    SYSCALL_WAITPID = 7,

    // Temporary: spawn a kernel-thread child for waitpid testing.
    SYSCALL_SPAWN = 8,
};

#endif
