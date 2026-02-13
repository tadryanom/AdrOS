// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#include "timer.h"
#include "console.h"
#include "process.h" 
#include "vdso.h"

#include "hal/timer.h"

static uint32_t tick = 0;

uint32_t get_tick_count(void) {
    return tick;
}

static void hal_tick_bridge(void) {
    tick++;
    vdso_update_tick(tick);
    process_wake_check(tick);
    schedule();
}

void timer_init(uint32_t frequency) {
    kprintf("[TIMER] Initializing...\n");
    hal_timer_init(frequency, hal_tick_bridge);
}
