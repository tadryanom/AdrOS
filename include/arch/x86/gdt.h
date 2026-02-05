// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#ifndef ARCH_X86_GDT_H
#define ARCH_X86_GDT_H

#include <stdint.h>
#include <stddef.h>

void gdt_init(void);
void tss_set_kernel_stack(uintptr_t esp0);

#endif
