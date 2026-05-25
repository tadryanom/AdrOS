// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#ifndef FAT_H
#define FAT_H

#include "fs.h"
#include "blockdev.h"
#include <stdint.h>

/* Mount a FAT12/16/32 filesystem on the given ATA drive starting at
 * the given LBA offset.  Auto-detects FAT type from BPB.
 * Returns a VFS root node or NULL on failure. */
fs_node_t* fat_mount(const block_device_t* bdev, uint32_t partition_lba);

#endif
