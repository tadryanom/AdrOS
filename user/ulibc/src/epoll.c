// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#include "sys/epoll.h"
#include "syscall.h"
#include "errno.h"

int epoll_create(int size) {
    (void)size;
    return __syscall_ret(_syscall1(SYS_EPOLL_CREATE, 0));
}

int epoll_create1(int flags) {
    return __syscall_ret(_syscall1(SYS_EPOLL_CREATE, flags));
}

int epoll_ctl(int epfd, int op, int fd, struct epoll_event* event) {
    return __syscall_ret(_syscall4(SYS_EPOLL_CTL, epfd, op, fd, (int)event));
}

int epoll_wait(int epfd, struct epoll_event* events, int maxevents, int timeout) {
    return __syscall_ret(_syscall4(SYS_EPOLL_WAIT, epfd, (int)events, maxevents, timeout));
}
