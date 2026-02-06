#ifndef ARCH_BOOT_ARGS_H
#define ARCH_BOOT_ARGS_H

#include <stdint.h>

struct arch_boot_args {
    uintptr_t a0;
    uintptr_t a1;
    uintptr_t a2;
    uintptr_t a3;
};

#endif
