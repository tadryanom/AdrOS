// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#ifndef TMPFS_H
#define TMPFS_H

#include "fs.h"
#include "blockdev.h"
#include <stdint.h>

struct block_device;

fs_node_t* tmpfs_create_root(void);
int tmpfs_add_file(fs_node_t* root_dir, const char* name, const uint8_t* data, uint32_t len);

/* VFS mount interface */
vfs_mount_result_t tmpfs_mount(struct block_device* bdev, uint32_t lba);
void tmpfs_kill_sb(vfs_superblock_t* sb);

int tmpfs_mkdir_p(fs_node_t* root_dir, const char* path);
fs_node_t* tmpfs_create_file(fs_node_t* root_dir, const char* path, const uint8_t* data, uint32_t len);
int tmpfs_create_symlink(fs_node_t* root_dir, const char* link_path, const char* target);

#endif
