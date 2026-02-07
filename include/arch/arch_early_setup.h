// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

 #ifndef ARCH_EARLY_SETUP_H
 #define ARCH_EARLY_SETUP_H

 #include "arch/arch_boot_args.h"

 void arch_early_setup(const struct arch_boot_args* args);

 #endif
