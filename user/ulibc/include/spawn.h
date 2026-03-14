// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef ULIBC_SPAWN_H
#define ULIBC_SPAWN_H

#include <stdint.h>
#include <signal.h>

typedef struct {
    int     __flags;
    int     __pgrp;
    sigset_t __sd;
    sigset_t __ss;
} posix_spawnattr_t;

typedef struct {
    int __allocated;
    int __used;
    void* __actions;
} posix_spawn_file_actions_t;

int posix_spawn(int* pid, const char* path,
                const posix_spawn_file_actions_t* file_actions,
                const posix_spawnattr_t* attrp,
                char* const argv[], char* const envp[]);

int posix_spawnp(int* pid, const char* file,
                 const posix_spawn_file_actions_t* file_actions,
                 const posix_spawnattr_t* attrp,
                 char* const argv[], char* const envp[]);

int posix_spawn_file_actions_init(posix_spawn_file_actions_t* fact);
int posix_spawn_file_actions_destroy(posix_spawn_file_actions_t* fact);
int posix_spawnattr_init(posix_spawnattr_t* attr);
int posix_spawnattr_destroy(posix_spawnattr_t* attr);

#endif
