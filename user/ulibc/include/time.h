// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef ULIBC_TIME_H
#define ULIBC_TIME_H

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>  /* for time_t */

struct timespec {
    time_t tv_sec;
    long   tv_nsec;
};

struct tm {
    int tm_sec;    /* seconds [0,60] */
    int tm_min;    /* minutes [0,59] */
    int tm_hour;   /* hours [0,23] */
    int tm_mday;   /* day of month [1,31] */
    int tm_mon;    /* month [0,11] */
    int tm_year;   /* years since 1900 */
    int tm_wday;   /* day of week [0,6] (Sun=0) */
    int tm_yday;   /* day of year [0,365] */
    int tm_isdst;  /* daylight savings flag */
};

#define CLOCK_REALTIME  0
#define CLOCK_MONOTONIC 1

int nanosleep(const struct timespec* req, struct timespec* rem);
int clock_gettime(int clk_id, struct timespec* tp);

time_t     time(time_t* tloc);
struct tm* localtime(const time_t* timep);
struct tm* gmtime(const time_t* timep);
char*      ctime(const time_t* timep);
size_t     strftime(char* s, size_t max, const char* fmt, const struct tm* tm);
char*      asctime(const struct tm* tm);
double     difftime(time_t time1, time_t time0);

#endif
