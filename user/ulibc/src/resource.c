// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#include "sys/resource.h"
#include "syscall.h"
#include "errno.h"

int getrlimit(int resource, struct rlimit *rlim) {
    return __syscall_ret(_syscall2(SYS_GETRLIMIT, resource, (int)rlim));
}

int setrlimit(int resource, const struct rlimit *rlim) {
    return __syscall_ret(_syscall2(SYS_SETRLIMIT, resource, (int)rlim));
}
