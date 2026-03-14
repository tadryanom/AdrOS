// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#ifndef ULIBC_MNTENT_H
#define ULIBC_MNTENT_H

#include <stdio.h>

struct mntent {
    char* mnt_fsname;   /* Device or server for filesystem */
    char* mnt_dir;      /* Directory mounted on */
    char* mnt_type;     /* Type of filesystem: ufs, nfs, etc. */
    char* mnt_opts;     /* Comma-separated options for fs */
    int   mnt_freq;     /* Dump frequency (in days) */
    int   mnt_passno;   /* Pass number for `fsck` */
};

FILE*          setmntent(const char* filename, const char* type);
struct mntent* getmntent(FILE* fp);
int            addmntent(FILE* fp, const struct mntent* mnt);
int            endmntent(FILE* fp);
char*          hasmntopt(const struct mntent* mnt, const char* opt);

#endif
