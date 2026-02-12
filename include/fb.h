// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#ifndef FB_H
#define FB_H

#include <stdint.h>

/*
 * Framebuffer ioctl commands (Linux-compatible subset).
 */
#define FBIOGET_VSCREENINFO  0x4600
#define FBIOGET_FSCREENINFO  0x4602

struct fb_var_screeninfo {
    uint32_t xres;
    uint32_t yres;
    uint32_t bits_per_pixel;
};

struct fb_fix_screeninfo {
    uint32_t smem_start;    /* physical address */
    uint32_t smem_len;      /* length of framebuffer mem */
    uint32_t line_length;   /* pitch (bytes per scanline) */
};

#endif
