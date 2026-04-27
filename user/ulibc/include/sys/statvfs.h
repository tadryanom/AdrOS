// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 */

#ifndef ULIBC_SYS_STATVFS_H
#define ULIBC_SYS_STATVFS_H

#include <sys/types.h>

struct statvfs {
    unsigned long f_bsize;    /* Filesystem block size */
    unsigned long f_frsize;   /* Fragment size */
    unsigned long f_blocks;   /* Size of fs in f_frsize units */
    unsigned long f_bfree;    /* Number of free blocks */
    unsigned long f_bavail;   /* Free blocks for unprivileged users */
    unsigned long f_files;    /* Number of inodes */
    unsigned long f_ffree;    /* Number of free inodes */
    unsigned long f_favail;   /* Free inodes for unprivileged users */
    unsigned long f_fsid;     /* Filesystem ID */
    unsigned long f_flag;     /* Mount flags */
    unsigned long f_namemax;  /* Maximum filename length */
};

int statvfs(const char* path, struct statvfs* buf);

#endif
