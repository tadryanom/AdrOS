// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef ULIBC_UTMP_H
#define ULIBC_UTMP_H

#include <stdint.h>

#define UT_LINESIZE 32
#define UT_NAMESIZE 32
#define UT_HOSTSIZE 256

#define EMPTY         0
#define RUN_LVL       1
#define BOOT_TIME     2
#define NEW_TIME      3
#define OLD_TIME      4
#define INIT_PROCESS  5
#define LOGIN_PROCESS 6
#define USER_PROCESS  7
#define DEAD_PROCESS  8

struct utmp {
    short   ut_type;
    int     ut_pid;
    char    ut_line[UT_LINESIZE];
    char    ut_id[4];
    char    ut_user[UT_NAMESIZE];
    char    ut_host[UT_HOSTSIZE];
    int32_t ut_tv_sec;
    int32_t ut_tv_usec;
};

void           setutent(void);
struct utmp*   getutent(void);
struct utmp*   getutid(const struct utmp* ut);
struct utmp*   getutline(const struct utmp* ut);
struct utmp*   pututline(const struct utmp* ut);
void           endutent(void);
void           utmpname(const char* file);

#endif
