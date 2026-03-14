// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#ifndef _SYS_REBOOT_H
#define _SYS_REBOOT_H
#define RB_AUTOBOOT    0x01234567
#define RB_HALT_SYSTEM 0xCDEF0123
#define RB_POWER_OFF   0x4321FEDC
int reboot(int cmd);
#endif
