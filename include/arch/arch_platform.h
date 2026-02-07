#ifndef ARCH_PLATFORM_H
#define ARCH_PLATFORM_H

#include "kernel/boot_info.h"

int arch_platform_setup(const struct boot_info* bi);
int arch_platform_start_userspace(const struct boot_info* bi);
void arch_platform_usermode_test_start(void);

#endif
