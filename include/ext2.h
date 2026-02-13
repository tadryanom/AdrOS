// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#ifndef EXT2_H
#define EXT2_H

#include "fs.h"
#include <stdint.h>

/* Mount an ext2 filesystem starting at the given LBA offset on disk.
 * Returns a VFS root node or NULL on failure. */
fs_node_t* ext2_mount(uint32_t partition_lba);

#endif
