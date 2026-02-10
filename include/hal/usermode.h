// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef HAL_USERMODE_H
#define HAL_USERMODE_H

#include <stdint.h>
#include "interrupts.h"

int hal_usermode_enter(uintptr_t user_eip, uintptr_t user_esp);

void hal_usermode_enter_regs(const struct registers* regs);

#endif
