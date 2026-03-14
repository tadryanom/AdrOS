// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef ULIBC_ERRNO_H
#define ULIBC_ERRNO_H

extern int errno;

#define EPERM            1
#define ENOENT           2
#define ESRCH            3
#define EINTR            4
#define EIO              5
#define ENXIO            6
#define E2BIG            7
#define EBADF            9
#define ECHILD          10
#define EAGAIN          11
#define ENOMEM          12
#define EACCES          13
#define EFAULT          14
#define EBUSY           16
#define EEXIST          17
#define ENODEV          19
#define ENOTDIR         20
#define EISDIR          21
#define EINVAL          22
#define EMFILE          24
#define ENOTTY          25
#define ENOSPC          28
#define ESPIPE          29
#define EROFS           30
#define EPIPE           32
#define ERANGE          34
#define ENAMETOOLONG    36
#define ENOLCK          37
#define ENOSYS          38
#define ENOTEMPTY       39
#define ELOOP           40
#define EAFNOSUPPORT    47
#define EADDRINUSE      48
#define ECONNRESET      54
#define ENOTCONN        57
#define ETIMEDOUT       60
#define ECONNREFUSED    61
#define EOVERFLOW       75
#define EMSGSIZE        90
#define ENOPROTOOPT     92
#define EPROTONOSUPPORT 93
#define EOPNOTSUPP      95
#define ENOTSOCK        88
#define ENETUNREACH     101
#define EWOULDBLOCK     EAGAIN

/* Convert raw syscall return to errno-style */
static inline int __syscall_ret(int r) {
    if (r < 0) {
        errno = -r;
        return -1;
    }
    return r;
}

#endif
