// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#include "aio.h"
#include "syscall.h"
#include "errno.h"

int aio_read(struct aiocb* aiocbp) {
    return __syscall_ret(_syscall3(SYS_AIO_READ, aiocbp->aio_fildes,
                                   (int)aiocbp->aio_buf, (int)aiocbp->aio_nbytes));
}

int aio_write(struct aiocb* aiocbp) {
    return __syscall_ret(_syscall3(SYS_AIO_WRITE, aiocbp->aio_fildes,
                                   (int)aiocbp->aio_buf, (int)aiocbp->aio_nbytes));
}

int aio_error(const struct aiocb* aiocbp) {
    return _syscall1(SYS_AIO_ERROR, (int)aiocbp);
}

int aio_return(struct aiocb* aiocbp) {
    return _syscall1(SYS_AIO_RETURN, (int)aiocbp);
}

int aio_suspend(const struct aiocb* const list[], int nent,
                const struct timespec* timeout) {
    return __syscall_ret(_syscall3(SYS_AIO_SUSPEND, (int)list, nent, (int)timeout));
}

int aio_cancel(int fd, struct aiocb* aiocbp) {
    (void)fd;
    (void)aiocbp;
    return AIO_ALLDONE;
}
