// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef ULIBC_FCNTL_H
#define ULIBC_FCNTL_H

#include <stdint.h>

#define O_RDONLY    0x0000
#define O_WRONLY    0x0001
#define O_RDWR      0x0002
#define O_CREAT     0x0040
#define O_EXCL      0x0080
#define O_NOCTTY    0x0100
#define O_TRUNC     0x0200
#define O_APPEND    0x0400
#define O_NONBLOCK  0x0800
#define O_DIRECTORY 0x10000
#define O_NOFOLLOW  0x20000
#define O_CLOEXEC   0x80000

#define F_DUPFD     0
#define F_GETFD     1
#define F_SETFD     2
#define F_GETFL     3
#define F_SETFL     4
#define F_GETLK     5
#define F_SETLK     6
#define F_SETLKW    7
#define F_DUPFD_CLOEXEC 1030
#define F_GETPIPE_SZ    1032
#define F_SETPIPE_SZ    1033

#define FD_CLOEXEC  1

/* AT_* constants for *at() syscalls */
#define AT_FDCWD         -100
#define AT_SYMLINK_NOFOLLOW 0x100
#define AT_REMOVEDIR     0x200
#define AT_SYMLINK_FOLLOW  0x400
#define AT_EACCESS       0x0400

/* Record lock types */
#define F_RDLCK     0
#define F_WRLCK     1
#define F_UNLCK     2

struct flock {
    int16_t  l_type;
    int16_t  l_whence;
    uint32_t l_start;
    uint32_t l_len;
    uint32_t l_pid;
};

int fcntl(int fd, int cmd, ...);

#endif
