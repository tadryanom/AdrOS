// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
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
