// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef FAT16_H
#define FAT16_H

#include "fs.h"
#include <stdint.h>

/* Mount a FAT16 filesystem starting at the given LBA offset on disk.
 * Returns a VFS root node or NULL on failure. */
fs_node_t* fat16_mount(uint32_t partition_lba);

#endif
