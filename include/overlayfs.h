// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef OVERLAYFS_H
#define OVERLAYFS_H

#include "fs.h"

fs_node_t* overlayfs_create_root(fs_node_t* lower_root, fs_node_t* upper_root);

#endif
