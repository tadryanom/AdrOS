// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#include "linux/futex.h"
#include "syscall.h"
#include <sys/time.h>

int futex(uint32_t* uaddr, int op, uint32_t val, const struct timeval* timeout) {
    return _syscall4(SYS_FUTEX, (int)uaddr, op, (int)val, (int)(uintptr_t)timeout);
}
