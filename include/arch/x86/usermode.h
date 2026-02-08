// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#ifndef ARCH_X86_USERMODE_H
#define ARCH_X86_USERMODE_H

#include <stdint.h>

#include "idt.h"

#if defined(__i386__)
__attribute__((noreturn)) void x86_enter_usermode(uintptr_t user_eip, uintptr_t user_esp);
__attribute__((noreturn)) void x86_enter_usermode_regs(const struct registers* regs);
#endif

#endif
