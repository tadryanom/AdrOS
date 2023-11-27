// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

/* 
 * Defines the interface for all PIT-related functions.
 * Based on code from JamesM's kernel development tutorials.
 * Rewritten by Tulio Mendes to AdrOS Kernel
 */

#ifndef __TIMER_H
#define __TIMER_H 1

#include <typedefs.h>

void init_timer (u32int);
void init_rtc (void);
void sleep_ms (u32int);

#endif
