// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#include "hal/video.h"

#if defined(__i386__) || defined(__x86_64__)

// This address must be mapped by VMM in higher half (see vmm_init mapping)
#define VGA_TEXT_BUFFER_VIRT ((uint16_t*)0xC00B8000)

uint16_t* hal_video_text_buffer(void) {
    return (uint16_t*)VGA_TEXT_BUFFER_VIRT;
}

#else
uint16_t* hal_video_text_buffer(void) {
    return (uint16_t*)0;
}
#endif
