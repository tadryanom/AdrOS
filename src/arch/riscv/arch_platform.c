#include "arch/arch_platform.h"

int arch_platform_setup(const struct boot_info* bi) {
    (void)bi;
    return -1;
}

int arch_platform_start_userspace(const struct boot_info* bi) {
    (void)bi;
    return -1;
}

void arch_platform_usermode_test_start(void) {
}
