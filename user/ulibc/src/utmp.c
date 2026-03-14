// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#include "utmp.h"
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
