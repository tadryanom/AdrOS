// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#include "spawn.h"
#include "syscall.h"
#include "errno.h"
#include "string.h"

int posix_spawn(int* pid, const char* path,
                const posix_spawn_file_actions_t* file_actions,
                const posix_spawnattr_t* attrp,
                char* const argv[], char* const envp[]) {
    (void)file_actions;
    (void)attrp;
    (void)envp;
    int ret = _syscall2(SYS_POSIX_SPAWN, (int)path, (int)argv);
    if (ret < 0) {
        errno = -ret;
        return -ret;
    }
    if (pid) *pid = ret;
    return 0;
}

int posix_spawnp(int* pid, const char* file,
                 const posix_spawn_file_actions_t* file_actions,
                 const posix_spawnattr_t* attrp,
                 char* const argv[], char* const envp[]) {
    return posix_spawn(pid, file, file_actions, attrp, argv, envp);
}

int posix_spawn_file_actions_init(posix_spawn_file_actions_t* fact) {
    memset(fact, 0, sizeof(*fact));
    return 0;
}

int posix_spawn_file_actions_destroy(posix_spawn_file_actions_t* fact) {
    (void)fact;
    return 0;
}

int posix_spawnattr_init(posix_spawnattr_t* attr) {
    memset(attr, 0, sizeof(*attr));
    return 0;
}

int posix_spawnattr_destroy(posix_spawnattr_t* attr) {
    (void)attr;
    return 0;
}
