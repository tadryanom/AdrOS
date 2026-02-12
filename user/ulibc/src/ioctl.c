// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#include "sys/ioctl.h"
#include "syscall.h"
#include "errno.h"

int ioctl(int fd, unsigned long cmd, void* arg) {
    return __syscall_ret(_syscall3(SYS_IOCTL, fd, (int)cmd, (int)arg));
}
