// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef VBE_H
#define VBE_H

#include <stdint.h>
#include "kernel/boot_info.h"

struct vbe_info {
    uintptr_t phys_addr;
    volatile uint8_t* virt_addr;
    uint32_t pitch;
    uint32_t width;
    uint32_t height;
    uint8_t  bpp;
    uint32_t size;
};

int  vbe_init(const struct boot_info* bi);
int  vbe_available(void);
const struct vbe_info* vbe_get_info(void);

void vbe_put_pixel(uint32_t x, uint32_t y, uint32_t color);
void vbe_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void vbe_clear(uint32_t color);

#endif
