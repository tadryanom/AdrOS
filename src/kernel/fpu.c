// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#include "arch_fpu.h"
#include "console.h"
#include "utils.h"

__attribute__((weak))
void arch_fpu_init(void) {
    kprintf("[FPU] No arch-specific FPU support.\n");
}

__attribute__((weak))
void arch_fpu_save(uint8_t* state) {
    (void)state;
}

__attribute__((weak))
void arch_fpu_restore(const uint8_t* state) {
    (void)state;
}

__attribute__((weak))
void arch_fpu_init_state(uint8_t* state) {
    memset(state, 0, FPU_STATE_SIZE);
}
