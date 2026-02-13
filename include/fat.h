// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef FAT_H
#define FAT_H

#include "fs.h"
#include <stdint.h>

/* Mount a FAT12/16/32 filesystem starting at the given LBA offset on disk.
 * Auto-detects FAT type from BPB.
 * Returns a VFS root node or NULL on failure. */
fs_node_t* fat_mount(uint32_t partition_lba);

#endif
