// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#include "utmp.h"
#include "heap.h"
#include "spinlock.h"
#include "string.h"
#include "console.h"
#include "process.h"
#include "timer.h"

#include <stddef.h>

#define UTMP_MAX_ENTRIES 32  /* L2: Reduced to avoid BSS overflow */

static struct utmp g_utmp[UTMP_MAX_ENTRIES];
static spinlock_t g_utmp_lock = {0};

void utmp_init(void) {
    for (int i = 0; i < UTMP_MAX_ENTRIES; i++) {
        memset(&g_utmp[i], 0, sizeof(g_utmp[i]));
        g_utmp[i].ut_type = EMPTY;
    }
}

static int utmp_get_time(struct utmp* u) {
    /* L2: Get current time (simplified - use timer ticks for now) */
    extern uint32_t get_tick_count(void);
    uint32_t ticks = get_tick_count();
    u->ut_tv.tv_sec = (int32_t)(ticks / TIMER_HZ);  /* Convert ticks to seconds */
    u->ut_tv.tv_usec = (int32_t)((ticks % TIMER_HZ) * (1000000 / TIMER_HZ));
    return 0;
}

int utmp_login(uint32_t pid, const char* line, const char* user, const char* host) {
    if (!line || !user) return -EINVAL;

    uintptr_t irqf = spin_lock_irqsave(&g_utmp_lock);

    /* Find empty slot */
    int slot = -1;
    for (int i = 0; i < UTMP_MAX_ENTRIES; i++) {
        if (g_utmp[i].ut_type == EMPTY) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        spin_unlock_irqrestore(&g_utmp_lock, irqf);
        return -ENOSPC;
    }

    struct utmp* u = &g_utmp[slot];
    memset(u, 0, sizeof(*u));

    u->ut_type = USER_PROCESS;
    u->ut_pid = pid;

    /* Copy line (device name) */
    strncpy(u->ut_line, line, sizeof(u->ut_line) - 1);
    u->ut_line[sizeof(u->ut_line) - 1] = '\0';

    /* Copy username */
    strncpy(u->ut_user, user, sizeof(u->ut_user) - 1);
    u->ut_user[sizeof(u->ut_user) - 1] = '\0';

    /* Copy hostname if provided */
    if (host) {
        strncpy(u->ut_host, host, sizeof(u->ut_host) - 1);
        u->ut_host[sizeof(u->ut_host) - 1] = '\0';
    }

    /* Set session ID (only if called from process context) */
    u->ut_session = 0;  /* L2: Simplified - no session tracking for now */

    /* Set timestamp */
    utmp_get_time(u);

    spin_unlock_irqrestore(&g_utmp_lock, irqf);
    return 0;
}

int utmp_logout(uint32_t pid, const char* line) {
    (void)line;  /* L2: Line parameter not used for logout by PID */
    uintptr_t irqf = spin_lock_irqsave(&g_utmp_lock);

    for (int i = 0; i < UTMP_MAX_ENTRIES; i++) {
        if (g_utmp[i].ut_type == USER_PROCESS && g_utmp[i].ut_pid == pid) {
            g_utmp[i].ut_type = DEAD_PROCESS;
            utmp_get_time(&g_utmp[i]);
            spin_unlock_irqrestore(&g_utmp_lock, irqf);
            return 0;
        }
    }

    spin_unlock_irqrestore(&g_utmp_lock, irqf);
    return -ESRCH;
}

int utmp_dead(uint32_t pid, int exit_status) {
    uintptr_t irqf = spin_lock_irqsave(&g_utmp_lock);

    for (int i = 0; i < UTMP_MAX_ENTRIES; i++) {
        if (g_utmp[i].ut_type == USER_PROCESS && g_utmp[i].ut_pid == pid) {
            g_utmp[i].ut_type = DEAD_PROCESS;
            g_utmp[i].ut_exit.e_exit = (short)(exit_status & 0xFF);
            g_utmp[i].ut_exit.e_termination = (short)((exit_status >> 8) & 0xFF);
            utmp_get_time(&g_utmp[i]);
            spin_unlock_irqrestore(&g_utmp_lock, irqf);
            return 0;
        }
    }

    spin_unlock_irqrestore(&g_utmp_lock, irqf);
    return -ESRCH;
}

struct utmp* utmp_get_by_pid(uint32_t pid) {
    uintptr_t irqf = spin_lock_irqsave(&g_utmp_lock);

    for (int i = 0; i < UTMP_MAX_ENTRIES; i++) {
        if (g_utmp[i].ut_type == USER_PROCESS && g_utmp[i].ut_pid == pid) {
            spin_unlock_irqrestore(&g_utmp_lock, irqf);
            return &g_utmp[i];
        }
    }

    spin_unlock_irqrestore(&g_utmp_lock, irqf);
    return NULL;
}
