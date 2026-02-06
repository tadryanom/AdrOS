#ifndef KERNEL_BOOT_INFO_H
#define KERNEL_BOOT_INFO_H

#include <stdint.h>
#include <stddef.h>

struct boot_info {
    uintptr_t arch_magic;
    uintptr_t arch_boot_info;

    uintptr_t initrd_start;
    uintptr_t initrd_end;

    const char* cmdline;
};

#endif
