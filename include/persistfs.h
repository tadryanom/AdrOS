// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#ifndef PERSISTFS_H
#define PERSISTFS_H

#include "fs.h"

fs_node_t* persistfs_create_root(int drive);

#endif
