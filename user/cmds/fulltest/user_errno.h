// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef USER_ERRNO_H
#define USER_ERRNO_H

extern int errno;

static inline int __syscall_fix(int ret) {
    if (ret < 0) {
        errno = -ret;
        return -1;
    }
    return ret;
}

#endif
