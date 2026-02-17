// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

/* AdrOS sleep utility — pause for N seconds */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

int main(int argc, char** argv) {
    if (argc <= 1) {
        fprintf(stderr, "usage: sleep SECONDS\n");
        return 1;
    }
    int secs = atoi(argv[1]);
    if (secs > 0) {
        struct timespec ts = { .tv_sec = secs, .tv_nsec = 0 };
        nanosleep(&ts, NULL);
    }
    return 0;
}
