// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#include "sys/mman.h"
#include "syscall.h"
#include "errno.h"

void* mmap(void* addr, size_t length, int prot, int flags, int fd, int offset) {
    int ret = _syscall5(SYS_MMAP, (int)addr, (int)length, prot, flags, fd);
    (void)offset;
    if (ret < 0 && ret > -4096) {
        errno = -ret;
        return MAP_FAILED;
    }
    return (void*)ret;
}

int munmap(void* addr, size_t length) {
    return __syscall_ret(_syscall2(SYS_MUNMAP, (int)addr, (int)length));
}
