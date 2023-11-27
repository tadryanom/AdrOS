// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#ifndef __KMAIN_H
#define __KMAIN_H 1

#include <typedefs.h>
#include <multiboot.h>
#include <screen.h>
#include <stdio.h>
#include <descriptors.h>
#include <timer.h>
#include <paging.h>
#include <kheap.h>
#include <system.h>
#include <initrd.h>
#include <string.h>
#include <task.h>

void kmain (u64int, u64int, u32int);

#endif
