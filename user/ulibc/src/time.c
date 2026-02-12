// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#include "time.h"
#include "syscall.h"
#include "errno.h"

int nanosleep(const struct timespec* req, struct timespec* rem) {
    return __syscall_ret(_syscall2(SYS_NANOSLEEP, (int)req, (int)rem));
}

int clock_gettime(int clk_id, struct timespec* tp) {
    return __syscall_ret(_syscall2(SYS_CLOCK_GETTIME, clk_id, (int)tp));
}
