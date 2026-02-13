// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#ifndef FAT16_H
#define FAT16_H

/* Legacy header — redirects to unified FAT driver */
#include "fat.h"

/* Backward compatibility: fat16_mount() is now fat_mount() */
static inline fs_node_t* fat16_mount(uint32_t partition_lba) {
    return fat_mount(partition_lba);
}

#endif
