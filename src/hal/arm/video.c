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

uint16_t* hal_video_text_buffer(void) {
    return (uint16_t*)0;
}

void hal_video_set_cursor(int row, int col) {
    (void)row; (void)col;
}
