// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

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
