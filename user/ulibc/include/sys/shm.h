// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef ULIBC_SYS_SHM_H
#define ULIBC_SYS_SHM_H

#include <stddef.h>
#include <stdint.h>

#define IPC_CREAT  01000
#define IPC_EXCL   02000
#define IPC_NOWAIT 04000
#define IPC_RMID   0
#define IPC_SET    1
#define IPC_STAT   2
#define IPC_PRIVATE 0

typedef int key_t;

struct shmid_ds {
    int    shm_segsz;
    int    shm_nattch;
    int    shm_cpid;
    int    shm_lpid;
    uint32_t shm_atime;
    uint32_t shm_dtime;
    uint32_t shm_ctime;
};

int   shmget(key_t key, size_t size, int shmflg);
void* shmat(int shmid, const void* shmaddr, int shmflg);
int   shmdt(const void* shmaddr);
int   shmctl(int shmid, int cmd, struct shmid_ds* buf);

#endif
