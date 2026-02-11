// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

/*
 * lwIP sys_arch for AdrOS — NO_SYS=1 mode.
 * Only sys_now() is required (for timeouts).
 */
#include "lwip/opt.h"
#include "lwip/sys.h"

extern uint32_t get_tick_count(void);

/* Return milliseconds since boot. Timer runs at 50 Hz → 20 ms per tick. */
u32_t sys_now(void) {
    return (u32_t)(get_tick_count() * 20);
}
