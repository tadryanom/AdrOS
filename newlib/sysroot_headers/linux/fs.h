// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef _LINUX_FS_H
#define _LINUX_FS_H
#define BLKGETSIZE   0x1260
#define BLKGETSIZE64 0x80041272
#define BLKFLSBUF    0x1261
#define BLKROSET     0x125D
#define BLKROGET     0x125E
#define BLKSSZGET    0x1268
#define BLKRRPART    0x125F
#define FIBMAP       1
#define FIGETBSZ     2
#define FS_IOC_GETFLAGS 0x80046601
#define FS_IOC_SETFLAGS 0x40046602
#endif
