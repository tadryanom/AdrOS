// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef HAL_MM_H
#define HAL_MM_H

#include <stdint.h>

#define HAL_MM_MAP_RW  (1u << 0)

int hal_mm_map_physical_range(uintptr_t phys_start, uintptr_t phys_end, uint32_t flags, uintptr_t* out_virt);

uintptr_t hal_mm_phys_to_virt(uintptr_t phys);
uintptr_t hal_mm_virt_to_phys(uintptr_t virt);
uintptr_t hal_mm_kernel_virt_base(void);

#endif
