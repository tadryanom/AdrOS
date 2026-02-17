// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

/* AdrOS date utility */
#include <stdio.h>
#include <time.h>

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) < 0) {
        fprintf(stderr, "date: cannot get time\n");
        return 1;
    }

    /* Simple epoch seconds display — no timezone or strftime yet */
    unsigned long sec = ts.tv_sec;
    unsigned long days = sec / 86400;
    unsigned long rem = sec % 86400;
    unsigned long hours = rem / 3600;
    unsigned long mins = (rem % 3600) / 60;
    unsigned long secs = rem % 60;

    printf("%lu days since epoch, %02lu:%02lu:%02lu UTC\n",
           days, hours, mins, secs);
    return 0;
}
