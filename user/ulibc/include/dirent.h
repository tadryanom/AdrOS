// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef ULIBC_DIRENT_H
#define ULIBC_DIRENT_H

#include <stdint.h>

struct dirent {
    uint32_t d_ino;
    uint16_t d_reclen;
    uint8_t  d_type;
    char     d_name[256];
};

#define DT_UNKNOWN 0
#define DT_REG     8
#define DT_DIR     4
#define DT_CHR     2
#define DT_BLK     6
#define DT_LNK    10

#endif
