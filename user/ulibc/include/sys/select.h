// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef ULIBC_SYS_SELECT_H
#define ULIBC_SYS_SELECT_H

#include <stdint.h>
#include "sys/time.h"

#define FD_SETSIZE 64

typedef struct {
    uint32_t fds_bits[FD_SETSIZE / 32];
} fd_set;

#define FD_ZERO(set)    do { for (int _i = 0; _i < (int)(FD_SETSIZE/32); _i++) (set)->fds_bits[_i] = 0; } while(0)
#define FD_SET(fd, set)   ((set)->fds_bits[(fd) / 32] |= (1U << ((fd) % 32)))
#define FD_CLR(fd, set)   ((set)->fds_bits[(fd) / 32] &= ~(1U << ((fd) % 32)))
#define FD_ISSET(fd, set) ((set)->fds_bits[(fd) / 32] & (1U << ((fd) % 32)))

int select(int nfds, fd_set* readfds, fd_set* writefds, fd_set* exceptfds,
           struct timeval* timeout);

#endif
