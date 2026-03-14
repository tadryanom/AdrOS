// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef _LINUX_KD_H
#define _LINUX_KD_H
#define KDGKBTYPE 0x4B33
#define KB_84     0x01
#define KB_101    0x02
#define KB_OTHER  0x03
#define KDSETMODE 0x4B3A
#define KDGETMODE 0x4B3B
#define KD_TEXT   0x00
#define KD_GRAPHICS 0x01
#endif
