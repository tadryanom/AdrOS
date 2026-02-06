// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#ifndef ARCH_START_H
#define ARCH_START_H

#include "arch/arch_boot_args.h"

void arch_start(const struct arch_boot_args* args);

#endif
