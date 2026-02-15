// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef ARCH_MIPS_SPINLOCK_H
#define ARCH_MIPS_SPINLOCK_H

#include <stdint.h>

static inline void cpu_relax(void) {
    __asm__ volatile("pause" ::: "memory");
}

static inline uintptr_t irq_save(void) {
    uintptr_t status;
    __asm__ volatile(
        "mfc0 %0, $12\n\t"
        "di"
        : "=r"(status) :: "memory");
    return status & 1U;
}

static inline void irq_restore(uintptr_t flags) {
    if (flags) {
        __asm__ volatile("ei" ::: "memory");
    }
}

#endif /* ARCH_MIPS_SPINLOCK_H */
