// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#include "poll.h"
#include "syscall.h"
#include "errno.h"

int poll(struct pollfd* fds, nfds_t nfds, int timeout) {
    int r = _syscall3(SYS_POLL, (int)fds, (int)nfds, timeout);
    return __syscall_ret(r);
}
