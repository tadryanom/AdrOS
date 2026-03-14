// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef ULIBC_SEMAPHORE_H
#define ULIBC_SEMAPHORE_H

#include <stdint.h>

typedef struct {
    int __val;
} sem_t;

#define SEM_FAILED ((sem_t*)-1)

sem_t* sem_open(const char* name, int oflag, ...);
int    sem_close(sem_t* sem);
int    sem_wait(sem_t* sem);
int    sem_trywait(sem_t* sem);
int    sem_post(sem_t* sem);
int    sem_unlink(const char* name);
int    sem_getvalue(sem_t* sem, int* sval);
int    sem_init(sem_t* sem, int pshared, unsigned int value);
int    sem_destroy(sem_t* sem);

#endif
