// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef _MNTENT_H
#define _MNTENT_H
#include <stdio.h>
struct mntent {
    char* mnt_fsname; char* mnt_dir; char* mnt_type;
    char* mnt_opts; int mnt_freq; int mnt_passno;
};
#define MOUNTED "/etc/mtab"
FILE* setmntent(const char* filename, const char* type);
struct mntent* getmntent(FILE* fp);
int endmntent(FILE* fp);
#endif
