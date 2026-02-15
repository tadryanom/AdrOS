// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef ARCH_RISCV_SPINLOCK_H
#define ARCH_RISCV_SPINLOCK_H

#include <stdint.h>

static inline void cpu_relax(void) {
    __asm__ volatile("fence" ::: "memory");
}

static inline uintptr_t irq_save(void) {
    uintptr_t mstatus;
    __asm__ volatile("csrrci %0, mstatus, 0x8" : "=r"(mstatus) :: "memory");
    return mstatus & 0x8;
}

static inline void irq_restore(uintptr_t flags) {
    if (flags) {
        __asm__ volatile("csrsi mstatus, 0x8" ::: "memory");
    }
}

#endif /* ARCH_RISCV_SPINLOCK_H */
