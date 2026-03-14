// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef _SYS_SYSMACROS_H
#define _SYS_SYSMACROS_H
#define major(dev) ((unsigned)((dev) >> 8) & 0xff)
#define minor(dev) ((unsigned)(dev) & 0xff)
#define makedev(maj,min) (((maj) << 8) | (min))
#endif
