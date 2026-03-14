// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#include "semaphore.h"
#include "syscall.h"
#include "errno.h"

sem_t* sem_open(const char* name, int oflag, ...) {
    int ret = _syscall2(SYS_SEM_OPEN, (int)name, oflag);
    if (ret < 0 && ret > -4096) {
        errno = -ret;
        return SEM_FAILED;
    }
    return (sem_t*)(long)ret;
}

int sem_close(sem_t* sem) {
    return __syscall_ret(_syscall1(SYS_SEM_CLOSE, (int)sem));
}

int sem_wait(sem_t* sem) {
    return __syscall_ret(_syscall1(SYS_SEM_WAIT, (int)sem));
}

int sem_trywait(sem_t* sem) {
    return __syscall_ret(_syscall1(SYS_SEM_WAIT, (int)sem));
}

int sem_post(sem_t* sem) {
    return __syscall_ret(_syscall1(SYS_SEM_POST, (int)sem));
}

int sem_unlink(const char* name) {
    return __syscall_ret(_syscall1(SYS_SEM_UNLINK, (int)name));
}

int sem_getvalue(sem_t* sem, int* sval) {
    return __syscall_ret(_syscall2(SYS_SEM_GETVALUE, (int)sem, (int)sval));
}

int sem_init(sem_t* sem, int pshared, unsigned int value) {
    (void)pshared;
    if (sem) sem->__val = (int)value;
    return 0;
}

int sem_destroy(sem_t* sem) {
    (void)sem;
    return 0;
}
