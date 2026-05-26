// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#ifndef PROCFS_H
#define PROCFS_H

#include "fs.h"
#include "blockdev.h"

struct block_device;

fs_node_t* procfs_create_root(void);

/* VFS mount interface */
vfs_mount_result_t procfs_mount(struct block_device* bdev, uint32_t lba);
void procfs_kill_sb(vfs_superblock_t* sb);

#endif
