// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#include "hal/timer.h"

void hal_timer_init(uint32_t frequency_hz, hal_timer_tick_cb_t tick_cb) {
    (void)frequency_hz;
    (void)tick_cb;
}
