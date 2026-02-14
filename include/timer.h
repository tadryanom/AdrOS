// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

#define TIMER_HZ           100
#define TIMER_MS_PER_TICK  (1000 / TIMER_HZ)   /* 10 ms */

void timer_init(uint32_t frequency);
uint32_t get_tick_count(void);

#endif
