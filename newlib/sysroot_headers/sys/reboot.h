// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef _SYS_REBOOT_H
#define _SYS_REBOOT_H
#define RB_AUTOBOOT    0x01234567
#define RB_HALT_SYSTEM 0xCDEF0123
#define RB_POWER_OFF   0x4321FEDC
int reboot(int cmd);
#endif
