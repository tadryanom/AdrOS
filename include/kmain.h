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

#include <typedefs.h>
#include <multiboot.h>
#include <screen.h>
#include <stdio.h>
#include <descriptors.h>

void kmain (u64int, u64int, u32int);

#endif
