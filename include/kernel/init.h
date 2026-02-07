// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#ifndef KERNEL_INIT_H
#define KERNEL_INIT_H

#include "kernel/boot_info.h"

void init_start(const struct boot_info* bi);

#endif
