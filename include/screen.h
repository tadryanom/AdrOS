// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef __SCREEN_H
#define __SCREEN_H 1

#include <common.h>

extern void scroll (void);
extern void move_cursor (void);
extern void init_video (void);
extern void cls (void);
extern void put_char (s8int);

#endif
