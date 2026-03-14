// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#include "sys/utsname.h"
#include "syscall.h"
#include "errno.h"

int uname(struct utsname* buf) {
    int r = _syscall1(SYS_UNAME, (int)buf);
    return __syscall_ret(r);
}
