// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
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

int getrusage(int who, struct rusage *usage) {
    return __syscall_ret(_syscall2(SYS_GETRUSAGE, who, (int)usage));
}
