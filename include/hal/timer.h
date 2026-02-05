// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef HAL_TIMER_H
#define HAL_TIMER_H

#include <stdint.h>

typedef void (*hal_timer_tick_cb_t)(void);

void hal_timer_init(uint32_t frequency_hz, hal_timer_tick_cb_t tick_cb);

#endif
