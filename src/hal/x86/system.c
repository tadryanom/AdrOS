// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#include "hal/system.h"

#if defined(__i386__) || defined(__x86_64__)
#include "io.h"
#include <stdint.h>

void hal_system_reboot(void) {
    uint8_t good = 0x02;
    while (good & 0x02) good = inb(0x64);
    outb(0x64, 0xFE);
}
#else
void hal_system_reboot(void) {
}
#endif
