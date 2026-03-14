// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef _SYS_SWAP_H
#define _SYS_SWAP_H
#define SWAP_FLAG_PREFER 0x8000
int swapon(const char* path, int flags);
int swapoff(const char* path);
#endif
