// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#include "unistd.h"
#include "time.h"
#include "errno.h"

unsigned int sleep(unsigned int seconds) {
    struct timespec req = { .tv_sec = seconds, .tv_nsec = 0 };
    struct timespec rem = { .tv_sec = 0, .tv_nsec = 0 };
    if (nanosleep(&req, &rem) < 0) {
        return (unsigned int)rem.tv_sec;
    }
    return 0;
}

int usleep(unsigned int usec) {
    struct timespec req;
    req.tv_sec = usec / 1000000;
    req.tv_nsec = (usec % 1000000) * 1000;
    return nanosleep(&req, (void*)0);
}
