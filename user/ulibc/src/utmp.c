// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#include "utmp.h"
#include "syscall.h"
#include "errno.h"
#include <stddef.h>

/* Stub implementation — AdrOS does not maintain utmp/wtmp records yet.
 * These stubs satisfy link-time dependencies for programs like login, w, who. */

static struct utmp _utmp_entry;

void setutent(void) {
    /* no-op */
}

struct utmp* getutent(void) {
    return NULL;  /* no entries */
}

struct utmp* getutid(const struct utmp* ut) {
    (void)ut;
    return NULL;
}

struct utmp* getutline(const struct utmp* ut) {
    (void)ut;
    return NULL;
}

struct utmp* pututline(const struct utmp* ut) {
    if (!ut) return NULL;
    _utmp_entry = *ut;
    return &_utmp_entry;
}

void endutent(void) {
    /* no-op */
}

void utmpname(const char* file) {
    (void)file;
}

/* Syscall wrappers for utmp management */
int utmp_login(uint32_t pid, const char* line, const char* user, const char* host) {
    return _syscall4(SYS_UTMP_LOGIN, (int)pid, (int)line, (int)user, (int)host);
}

int utmp_logout(uint32_t pid, const char* line) {
    return _syscall2(SYS_UTMP_LOGOUT, (int)pid, (int)line);
}

int utmp_dead(uint32_t pid, int exit_status) {
    return _syscall2(SYS_UTMP_DEAD, (int)pid, exit_status);
}
