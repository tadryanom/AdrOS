// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef ARCH_X86_PERCPU_H
#define ARCH_X86_PERCPU_H

#include <stdint.h>

/* Forward declarations */
struct process;
struct runqueue;

/* Per-CPU data block — one per CPU, accessed via GS segment.
 * The GS base for each CPU points to its own percpu_data instance. */
struct percpu_data {
    struct percpu_data* self;         /* offset 0 — self-pointer for percpu_get() */
    uint32_t         cpu_index;       /* offset 4, 0 = BSP */
    uint32_t         lapic_id;        /* offset 8 */
    struct process*  current_process; /* offset 12 */
    uintptr_t        kernel_stack;    /* offset 16 */
    uint32_t         nested_irq;      /* offset 20 */
    uint32_t         rq_load;         /* offset 24 */
    int              uaccess_active;  /* offset 28 */
    int              uaccess_faulted; /* offset 32 */
    uintptr_t        uaccess_recover; /* offset 36 */
};

/* Initialize per-CPU data for all CPUs. Called once from BSP after SMP init. */
void percpu_init(void);

/* Set up GS segment for the current CPU (called by each CPU during init). */
void percpu_setup_gs(uint32_t cpu_index);

/* Get pointer to current CPU's percpu_data (via GS segment). */
static inline struct percpu_data* percpu_get(void) {
    struct percpu_data* p;
    __asm__ volatile("mov %%gs:0, %0" : "=r"(p));
    return p;
}

/* Get current CPU index (fast path via GS). */
static inline uint32_t percpu_cpu_index(void) {
    uint32_t idx;
    __asm__ volatile("mov %%gs:4, %0" : "=r"(idx));
    return idx;
}

/* Get current process on this CPU (fast path via GS). */
static inline struct process* percpu_current(void) {
    struct process* p;
    __asm__ volatile("mov %%gs:12, %0" : "=r"(p));
    return p;
}

/* Set current process on this CPU. */
static inline void percpu_set_current(struct process* proc) {
    __asm__ volatile("mov %0, %%gs:12" : : "r"(proc) : "memory");
}

#endif
