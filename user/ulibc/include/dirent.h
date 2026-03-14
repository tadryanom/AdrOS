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
#include <stddef.h>

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

typedef struct _DIR {
    int   fd;
    int   pos;
    int   len;
    char  buf[4096];
} DIR;

DIR*           opendir(const char* name);
struct dirent* readdir(DIR* dirp);
int            closedir(DIR* dirp);
void           rewinddir(DIR* dirp);

#endif
