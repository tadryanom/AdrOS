// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef ARCH_X86_SMP_H
#define ARCH_X86_SMP_H

#include <stdint.h>

/* Maximum number of CPUs supported */
#define SMP_MAX_CPUS 16

/* Per-CPU state */
struct cpu_info {
    uint8_t  lapic_id;       /* LAPIC ID */
    uint8_t  cpu_index;      /* Index in cpu_info array (0 = BSP) */
    uint8_t  started;        /* 1 if AP has completed init */
    uint8_t  reserved;
    uint32_t kernel_stack;   /* Top of this CPU's kernel stack */
};

/* Phase 1: Discover CPUs from ACPI MADT and populate cpu_info.
 * Does NOT send SIPI. Returns number of CPUs found. */
int smp_enumerate(void);

/* Phase 2: Send INIT-SIPI-SIPI to wake APs.
 * Must be called after percpu_init() so GDT entries exist.
 * Returns number of CPUs that started (including BSP). */
int smp_start_aps(void);

/* Get the number of active CPUs. */
uint32_t smp_get_cpu_count(void);

/* Get cpu_info for a given CPU index. */
const struct cpu_info* smp_get_cpu(uint32_t index);

/* Get the current CPU's index (based on LAPIC ID). */
uint32_t smp_current_cpu(void);

#endif
