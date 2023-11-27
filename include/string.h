// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef __STRING_H
#define __STRING_H 1

#include <typedefs.h>

u8int *memcpy (u8int *, const u8int *, s32int);
u8int *memset (u8int *, u8int, s32int);
u16int *memsetw (u16int *, u16int, s32int);
s32int strlen (const s8int *);

#endif
