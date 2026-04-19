// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef ULIBC_SYS_MOUNT_H
#define ULIBC_SYS_MOUNT_H

#include <stddef.h>

/* Mount flags */
#define MS_RDONLY      1
#define MS_NOSUID      2
#define MS_NODEV       4
#define MS_NOEXEC      8
#define MS_SYNCHRONOUS 16
#define MS_REMOUNT     32
#define MS_NOATIME     1024

/* Umount flags */
#define MNT_FORCE      1
#define MNT_DETACH     2
#define MNT_EXPIRE     4

int mount(const char* source, const char* target, const char* fs_type,
          unsigned long flags, const void* data);
int umount2(const char* target, int flags);
int umount(const char* target);

#endif
