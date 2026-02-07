// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#ifndef INITRD_H
#define INITRD_H

#include "fs.h"
#include <stdint.h>

// Initialize InitRD and return the root node (directory)
fs_node_t* initrd_init(uint32_t location);

#endif
