// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#ifndef _SYS_SYSMACROS_H
#define _SYS_SYSMACROS_H
#define major(dev) ((unsigned)((dev) >> 8) & 0xff)
#define minor(dev) ((unsigned)(dev) & 0xff)
#define makedev(maj,min) (((maj) << 8) | (min))
#endif
