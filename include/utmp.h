// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#ifndef UTMP_H
#define UTMP_H

#include <stdint.h>

/* utmp record types (Linux-compatible) */
#define EMPTY           0
#define RUN_LVL         1
#define BOOT_TIME       2
#define NEW_TIME        3
#define OLD_TIME        4
#define INIT_PROCESS    5
#define LOGIN_PROCESS   6
#define USER_PROCESS    7
#define DEAD_PROCESS    8

/* utmp structure (simplified, Linux-compatible subset) */
struct utmp {
    short   ut_type;              /* Type of record */
    uint32_t ut_pid;              /* PID of login process */
    char    ut_line[32];          /* Device name of tty - "/dev/" */
    char    ut_id[4];             /* Terminal name suffix, or inittab(5) ID */
    char    ut_user[32];          /* Username */
    char    ut_host[256];         /* Hostname for remote login */
    struct  exit_status {
        short e_termination;      /* Process termination status */
        short e_exit;             /* Process exit status */
    } ut_exit;                    /* Exit status of a process marked as DEAD_PROCESS */
    int32_t ut_session;          /* Session ID (getsid(2)), used for windowing */
    struct {
        int32_t tv_sec;           /* Seconds */
        int32_t tv_usec;          /* Microseconds */
    } ut_tv;                      /* Time entry was made */
    int32_t ut_addr_v6[4];        /* Internet address of remote host */
    char    __unused[20];         /* Reserved for future use */
};

/* Kernel API for utmp management */
void utmp_init(void);
int utmp_login(uint32_t pid, const char* line, const char* user, const char* host);
int utmp_logout(uint32_t pid, const char* line);
int utmp_dead(uint32_t pid, int exit_status);
struct utmp* utmp_get_by_pid(uint32_t pid);

#endif /* UTMP_H */
