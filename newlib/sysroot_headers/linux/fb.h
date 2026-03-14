// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef _LINUX_FB_H
#define _LINUX_FB_H
#include <stdint.h>
struct fb_fix_screeninfo {
    char id[16]; unsigned long smem_start; uint32_t smem_len;
    uint32_t type; uint32_t visual; uint16_t xpanstep; uint16_t ypanstep;
    uint16_t ywrapstep; uint32_t line_length; unsigned long mmio_start;
    uint32_t mmio_len; uint32_t accel;
};
struct fb_bitfield { uint32_t offset; uint32_t length; uint32_t msb_right; };
struct fb_var_screeninfo {
    uint32_t xres; uint32_t yres; uint32_t xres_virtual; uint32_t yres_virtual;
    uint32_t xoffset; uint32_t yoffset; uint32_t bits_per_pixel; uint32_t grayscale;
    struct fb_bitfield red; struct fb_bitfield green; struct fb_bitfield blue;
    struct fb_bitfield transp;
};
#define FBIOGET_VSCREENINFO 0x4600
#define FBIOPUT_VSCREENINFO 0x4601
#define FBIOGET_FSCREENINFO 0x4602
#endif
