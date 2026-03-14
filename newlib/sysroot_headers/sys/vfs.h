// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef _SYS_VFS_H
#define _SYS_VFS_H
#include <sys/types.h>
struct statfs {
    long f_type; long f_bsize; long f_blocks; long f_bfree;
    long f_bavail; long f_files; long f_ffree; long f_fsid;
    long f_namelen;
};
int statfs(const char* path, struct statfs* buf);
int fstatfs(int fd, struct statfs* buf);
#endif
