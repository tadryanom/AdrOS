// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#ifndef ULIBC_SYS_UIO_H
#define ULIBC_SYS_UIO_H

#include <stddef.h>

struct iovec {
    void*   iov_base;
    size_t  iov_len;
};

int readv(int fd, const struct iovec* iov, int iovcnt);
int writev(int fd, const struct iovec* iov, int iovcnt);

#endif
