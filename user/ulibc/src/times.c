// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#include "sys/times.h"
#include "syscall.h"

uint32_t times(struct tms* buf) {
    return (uint32_t)_syscall1(SYS_TIMES, (int)buf);
}
