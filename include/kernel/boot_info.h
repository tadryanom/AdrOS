// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef KERNEL_BOOT_INFO_H
#define KERNEL_BOOT_INFO_H

#include <stdint.h>
#include <stddef.h>

struct boot_info {
    uintptr_t arch_magic;
    uintptr_t arch_boot_info;

    uintptr_t initrd_start;
    uintptr_t initrd_end;

    const char* cmdline;

    uintptr_t fb_addr;
    uint32_t  fb_pitch;
    uint32_t  fb_width;
    uint32_t  fb_height;
    uint8_t   fb_bpp;
};

#endif
