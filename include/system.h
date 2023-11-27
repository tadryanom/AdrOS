// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef __SYSTEM_H
#define __SYSTEM_H 1

#include <typedefs.h>

u8int inportb (u16int);
u16int inportw (u16int);
void outportb (u16int, u8int);
void outportw (u16int, u16int);

#endif
