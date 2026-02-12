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

struct timespec {
    uint32_t tv_sec;
    uint32_t tv_nsec;
};

#define CLOCK_REALTIME  0
#define CLOCK_MONOTONIC 1

int nanosleep(const struct timespec* req, struct timespec* rem);
int clock_gettime(int clk_id, struct timespec* tp);

#endif
