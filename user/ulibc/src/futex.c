// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#include "linux/futex.h"
#include "syscall.h"

int futex(uint32_t* uaddr, int op, uint32_t val) {
    return _syscall3(SYS_FUTEX, (int)uaddr, op, (int)val);
}
