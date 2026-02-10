// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#include "elf.h"
#include <stdint.h>

__attribute__((weak))
int elf32_load_user_from_initrd(const char* filename, uintptr_t* entry_out, uintptr_t* user_stack_top_out, uintptr_t* addr_space_out, uintptr_t* heap_break_out) {
    (void)filename;
    (void)entry_out;
    (void)user_stack_top_out;
    (void)addr_space_out;
    (void)heap_break_out;
    return -1;
}
