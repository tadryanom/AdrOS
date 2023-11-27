// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#ifndef __SCREEN_H
#define __SCREEN_H 1

#include <typedefs.h>

void init_video (void);
void cls (void);
void put_char (s8int);

#endif
