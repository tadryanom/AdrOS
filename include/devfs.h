// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#ifndef DEVFS_H
#define DEVFS_H

#include "fs.h"
#include "blockdev.h"

#define DEVFS_MAX_DEVICES 32

struct block_device;

fs_node_t* devfs_create_root(void);

/* VFS mount interface */
vfs_mount_result_t devfs_mount(struct block_device* bdev, uint32_t lba);
void devfs_kill_sb(vfs_superblock_t* sb);

/*
 * Register a device node with devfs.
 * The caller owns the fs_node_t storage (must remain valid for the
 * lifetime of the device).  DevFS stores only a pointer.
 * Returns 0 on success, -1 if the registry is full.
 */
int devfs_register_device(fs_node_t *node);

/* Register partition devices (placeholder for future implementation) */
void devfs_register_partitions(void);

#endif
