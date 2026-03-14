// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef ULIBC_AIO_H
#define ULIBC_AIO_H

#include <stddef.h>
#include <signal.h>
#include <time.h>

struct aiocb {
    int             aio_fildes;
    volatile void*  aio_buf;
    size_t          aio_nbytes;
    int             aio_offset;
    int             aio_reqprio;
    int             aio_lio_opcode;
    /* internal */
    int             __error;
    int             __return;
};

#define AIO_CANCELED    0
#define AIO_NOTCANCELED 1
#define AIO_ALLDONE     2

int aio_read(struct aiocb* aiocbp);
int aio_write(struct aiocb* aiocbp);
int aio_error(const struct aiocb* aiocbp);
int aio_return(struct aiocb* aiocbp);
int aio_suspend(const struct aiocb* const list[], int nent,
                const struct timespec* timeout);
int aio_cancel(int fd, struct aiocb* aiocbp);

#endif
