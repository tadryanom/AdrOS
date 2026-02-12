// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#include "sys/uio.h"
#include "syscall.h"
#include "errno.h"

int readv(int fd, const struct iovec* iov, int iovcnt) {
    return __syscall_ret(_syscall3(SYS_READV, fd, (int)iov, iovcnt));
}

int writev(int fd, const struct iovec* iov, int iovcnt) {
    return __syscall_ret(_syscall3(SYS_WRITEV, fd, (int)iov, iovcnt));
}
