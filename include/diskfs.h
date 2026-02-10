// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#ifndef DISKFS_H
#define DISKFS_H

#include "fs.h"
#include <stdint.h>

fs_node_t* diskfs_create_root(void);

// Open (and optionally create) a diskfs file at the root (flat namespace).
// rel_path must not contain '/'.
// flags: supports O_CREAT (0x40) and O_TRUNC (0x200) semantics (minimal).
int diskfs_open_file(const char* rel_path, uint32_t flags, fs_node_t** out_node);

#endif
