// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#ifndef HAL_VIDEO_H
#define HAL_VIDEO_H

#include <stdint.h>

uint16_t* hal_video_text_buffer(void);
void hal_video_set_cursor(int row, int col);

#endif
