// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#ifndef HAL_USERMODE_H
#define HAL_USERMODE_H

#include <stdint.h>

int hal_usermode_enter(uintptr_t user_eip, uintptr_t user_esp);

void hal_usermode_enter_regs(const void* regs);

#endif
