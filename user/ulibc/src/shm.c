// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#include "sys/shm.h"
#include "syscall.h"
#include "errno.h"

int shmget(key_t key, size_t size, int shmflg) {
    return __syscall_ret(_syscall3(SYS_SHMGET, key, (int)size, shmflg));
}

void* shmat(int shmid, const void* shmaddr, int shmflg) {
    int ret = _syscall3(SYS_SHMAT, shmid, (int)shmaddr, shmflg);
    if (ret < 0 && ret > -4096) {
        errno = -ret;
        return (void*)-1;
    }
    return (void*)ret;
}

int shmdt(const void* shmaddr) {
    return __syscall_ret(_syscall1(SYS_SHMDT, (int)shmaddr));
}

int shmctl(int shmid, int cmd, struct shmid_ds* buf) {
    return __syscall_ret(_syscall3(SYS_SHMCTL, shmid, cmd, (int)buf));
}
