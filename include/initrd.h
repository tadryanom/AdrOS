// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef INITRD_H
#define INITRD_H

#include "fs.h"
#include <stdint.h>

typedef struct {
    uint8_t magic; // Magic number 0xBF
    char name[64];
    uint32_t offset; // Offset relative to start of initrd
    uint32_t length;
} initrd_file_header_t;

// Initialize InitRD and return the root node (directory)
fs_node_t* initrd_init(uint32_t location);

#endif
