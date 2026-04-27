// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#include "time.h"
#include "sys/time.h"
#include "syscall.h"
#include "errno.h"
#include "string.h"
#include "stdio.h"

static int _is_leap(int y) {
    return ((y % 4) == 0 && (y % 100) != 0) || ((y % 400) == 0);
}

static int _days_in_month(int m, int y) {
    static const int d[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (m == 1 && _is_leap(y)) return 29;
    return d[m];
}

static struct tm _tm_buf;

time_t time(time_t* tloc) {
    struct timeval tv;
    if (gettimeofday(&tv, (void*)0) < 0) {
        if (tloc) *tloc = 0;
        return 0;
    }
    if (tloc) *tloc = (time_t)tv.tv_sec;
    return (time_t)tv.tv_sec;
}

struct tm* gmtime(const time_t* timep) {
    if (!timep) return (struct tm*)0;
    time_t t = *timep;
    int days = (int)(t / 86400);
    int secs = (int)(t % 86400);
    if (secs < 0) { secs += 86400; days--; }

    _tm_buf.tm_sec  = secs % 60;
    _tm_buf.tm_min  = (secs / 60) % 60;
    _tm_buf.tm_hour = secs / 3600;
    _tm_buf.tm_wday = (days + 4) % 7;  /* Jan 1 1970 = Thursday */
    if (_tm_buf.tm_wday < 0) _tm_buf.tm_wday += 7;

    /* Compute year */
    int y = 1970;
    for (;;) {
        int dy = _is_leap(y) ? 366 : 365;
        if (days < dy) break;
        days -= dy;
        y++;
    }
    _tm_buf.tm_year = y - 1900;
    _tm_buf.tm_yday = days;

    /* Compute month */
    int m = 0;
    for (;;) {
        int dm = _days_in_month(m, y);
        if (days < dm) break;
        days -= dm;
        m++;
    }
    _tm_buf.tm_mon  = m;
    _tm_buf.tm_mday = days + 1;
    _tm_buf.tm_isdst = 0;
    return &_tm_buf;
}

struct tm* localtime(const time_t* timep) {
    /* AdrOS has no timezone support, same as gmtime */
    return gmtime(timep);
}

char* asctime(const struct tm* tm) {
    static char buf[26];
    static const char* wday[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    static const char* mon[] = {"Jan","Feb","Mar","Apr","May","Jun",
                                "Jul","Aug","Sep","Oct","Nov","Dec"};
    if (!tm) return (char*)0;
    snprintf(buf, sizeof(buf), "%.3s %.3s%3d %02d:%02d:%02d %d\n",
             wday[tm->tm_wday >= 0 && tm->tm_wday < 7 ? tm->tm_wday : 0],
             mon[tm->tm_mon >= 0 && tm->tm_mon < 12 ? tm->tm_mon : 0],
             tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec,
             1900 + tm->tm_year);
    return buf;
}

char* ctime(const time_t* timep) {
    return asctime(localtime(timep));
}

double difftime(time_t time1, time_t time0) {
    return (double)time1 - (double)time0;
}

size_t strftime(char* s, size_t max, const char* fmt, const struct tm* tm) {
    if (!s || !fmt || !tm || max == 0) return 0;
    static const char* wday[] = {"Sunday","Monday","Tuesday","Wednesday",
                                  "Thursday","Friday","Saturday"};
    static const char* wday_s[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    static const char* mon[] = {"January","February","March","April","May","June",
                                "July","August","September","October","November","December"};
    static const char* mon_s[] = {"Jan","Feb","Mar","Apr","May","Jun",
                                  "Jul","Aug","Sep","Oct","Nov","Dec"};
    size_t pos = 0;
#define PUTC(c) do { if (pos + 1 < max) s[pos] = (c); pos++; } while(0)
#define PUTS(str) do { const char* _p = (str); while (*_p) { PUTC(*_p); _p++; } } while(0)

    while (*fmt && pos < max) {
        if (*fmt != '%') { PUTC(*fmt); fmt++; continue; }
        fmt++;
        char tmp[16];
        switch (*fmt) {
        case 'Y':
            snprintf(tmp, sizeof(tmp), "%d", 1900 + tm->tm_year);
            PUTS(tmp);
            break;
        case 'm':
            snprintf(tmp, sizeof(tmp), "%02d", tm->tm_mon + 1);
            PUTS(tmp);
            break;
        case 'd':
            snprintf(tmp, sizeof(tmp), "%02d", tm->tm_mday);
            PUTS(tmp);
            break;
        case 'H':
            snprintf(tmp, sizeof(tmp), "%02d", tm->tm_hour);
            PUTS(tmp);
            break;
        case 'M':
            snprintf(tmp, sizeof(tmp), "%02d", tm->tm_min);
            PUTS(tmp);
            break;
        case 'S':
            snprintf(tmp, sizeof(tmp), "%02d", tm->tm_sec);
            PUTS(tmp);
            break;
        case 'b': PUTS(mon_s[tm->tm_mon >= 0 && tm->tm_mon < 12 ? tm->tm_mon : 0]); break;
        case 'B': PUTS(mon[tm->tm_mon >= 0 && tm->tm_mon < 12 ? tm->tm_mon : 0]); break;
        case 'a': PUTS(wday_s[tm->tm_wday >= 0 && tm->tm_wday < 7 ? tm->tm_wday : 0]); break;
        case 'A': PUTS(wday[tm->tm_wday >= 0 && tm->tm_wday < 7 ? tm->tm_wday : 0]); break;
        case 'e':
            snprintf(tmp, sizeof(tmp), "%2d", tm->tm_mday);
            PUTS(tmp);
            break;
        case 'j':
            snprintf(tmp, sizeof(tmp), "%03d", tm->tm_yday + 1);
            PUTS(tmp);
            break;
        case 'w':
            PUTC('0' + tm->tm_wday);
            break;
        case 'I':
            snprintf(tmp, sizeof(tmp), "%02d",
                     tm->tm_hour == 0 ? 12 : (tm->tm_hour > 12 ? tm->tm_hour - 12 : tm->tm_hour));
            PUTS(tmp);
            break;
        case 'p':
            PUTS(tm->tm_hour < 12 ? "AM" : "PM");
            break;
        case 'c':
            /* Date+time: Thu Aug 23 14:55:02 2001 */
            PUTS(wday_s[tm->tm_wday >= 0 && tm->tm_wday < 7 ? tm->tm_wday : 0]);
            PUTC(' ');
            PUTS(mon_s[tm->tm_mon >= 0 && tm->tm_mon < 12 ? tm->tm_mon : 0]);
            PUTC(' ');
            snprintf(tmp, sizeof(tmp), "%2d", tm->tm_mday);
            PUTS(tmp);
            PUTC(' ');
            snprintf(tmp, sizeof(tmp), "%02d:%02d:%02d", tm->tm_hour, tm->tm_min, tm->tm_sec);
            PUTS(tmp);
            PUTC(' ');
            snprintf(tmp, sizeof(tmp), "%d", 1900 + tm->tm_year);
            PUTS(tmp);
            break;
        case 'F':
            snprintf(tmp, sizeof(tmp), "%d-%02d-%02d",
                     1900 + tm->tm_year, tm->tm_mon + 1, tm->tm_mday);
            PUTS(tmp);
            break;
        case 'T':
            snprintf(tmp, sizeof(tmp), "%02d:%02d:%02d",
                     tm->tm_hour, tm->tm_min, tm->tm_sec);
            PUTS(tmp);
            break;
        case 'R':
            snprintf(tmp, sizeof(tmp), "%02d:%02d", tm->tm_hour, tm->tm_min);
            PUTS(tmp);
            break;
        case 'D':
            snprintf(tmp, sizeof(tmp), "%02d/%02d/%02d",
                     tm->tm_mon + 1, tm->tm_mday, (1900 + tm->tm_year) % 100);
            PUTS(tmp);
            break;
        case '%':
            PUTC('%');
            break;
        default:
            PUTC('%');
            if (*fmt) PUTC(*fmt);
            break;
        }
        if (*fmt) fmt++;
    }
    if (pos < max) s[pos] = '\0';
    else if (max > 0) s[max - 1] = '\0';
    return pos;
#undef PUTC
#undef PUTS
}

int nanosleep(const struct timespec* req, struct timespec* rem) {
    return __syscall_ret(_syscall2(SYS_NANOSLEEP, (int)req, (int)rem));
}

int clock_gettime(int clk_id, struct timespec* tp) {
    return __syscall_ret(_syscall2(SYS_CLOCK_GETTIME, clk_id, (int)tp));
}

int gettimeofday(struct timeval* tv, void* tz) {
    (void)tz;
    return __syscall_ret(_syscall2(SYS_GETTIMEOFDAY, (int)tv, 0));
}
