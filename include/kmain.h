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

#include <common.h>
#include <multiboot.h>
#include <system.h>
#include <screen.h>
#include <stdio.h>

extern void kmain (u64int, u64int);

#endif
