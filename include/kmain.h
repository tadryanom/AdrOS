// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef __KMAIN_H
#define __KMAIN_H 1

#include <common.h>
#include <multiboot.h>
#include <system.h>
#include <screen.h>
#include <stdio.h>

/* Check if the bit BIT in FLAGS is set. */
#define CHECK_FLAG(flags,bit) ((flags) & (1 << (bit)))

/* Multiboot Info */
multiboot_info_t *mbi;

/* Forward declarations. */
extern void kmain (u64int, u64int);
extern void printmbi(void);
extern void putpixel(s32int pos_x, s32int pos_y, s32int color);

#endif
