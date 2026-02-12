// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef MTRR_H
#define MTRR_H

#include <stdint.h>

#define MTRR_TYPE_UC  0  /* Uncacheable */
#define MTRR_TYPE_WC  1  /* Write-Combining */
#define MTRR_TYPE_WT  4  /* Write-Through */
#define MTRR_TYPE_WP  5  /* Write-Protect */
#define MTRR_TYPE_WB  6  /* Write-Back */

void mtrr_init(void);
int  mtrr_set_range(uint64_t base, uint64_t size, uint8_t type);

#endif
