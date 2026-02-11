// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef TMPFS_H
#define TMPFS_H

#include "fs.h"
#include <stdint.h>

fs_node_t* tmpfs_create_root(void);
int tmpfs_add_file(fs_node_t* root_dir, const char* name, const uint8_t* data, uint32_t len);

int tmpfs_mkdir_p(fs_node_t* root_dir, const char* path);
fs_node_t* tmpfs_create_file(fs_node_t* root_dir, const char* path, const uint8_t* data, uint32_t len);
int tmpfs_create_symlink(fs_node_t* root_dir, const char* link_path, const char* target);

#endif
