// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef _SYS_DIRENT_H
#define _SYS_DIRENT_H

#include <sys/types.h>

#define MAXNAMLEN 255

struct dirent {
    ino_t  d_ino;
    char   d_name[MAXNAMLEN + 1];
};

typedef struct {
    int    dd_fd;
    int    dd_loc;
    int    dd_size;
    char  *dd_buf;
} DIR;

DIR *opendir(const char *);
struct dirent *readdir(DIR *);
int closedir(DIR *);
void rewinddir(DIR *);

#endif /* _SYS_DIRENT_H */
