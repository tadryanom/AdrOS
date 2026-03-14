// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#include "sys/inotify.h"
#include "syscall.h"
#include "errno.h"

int inotify_init(void) {
    return __syscall_ret(_syscall0(SYS_INOTIFY_INIT));
}

int inotify_init1(int flags) {
    return __syscall_ret(_syscall1(SYS_INOTIFY_INIT, flags));
}

int inotify_add_watch(int fd, const char* pathname, uint32_t mask) {
    return __syscall_ret(_syscall3(SYS_INOTIFY_ADD_WATCH, fd, (int)pathname, (int)mask));
}

int inotify_rm_watch(int fd, int wd) {
    return __syscall_ret(_syscall2(SYS_INOTIFY_RM_WATCH, fd, wd));
}
