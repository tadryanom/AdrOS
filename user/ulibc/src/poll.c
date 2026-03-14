// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#include "poll.h"
#include "syscall.h"
#include "errno.h"

int poll(struct pollfd* fds, nfds_t nfds, int timeout) {
    int r = _syscall3(SYS_POLL, (int)fds, (int)nfds, timeout);
    return __syscall_ret(r);
}
