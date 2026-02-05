// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef HAL_KEYBOARD_H
#define HAL_KEYBOARD_H

typedef void (*hal_keyboard_char_cb_t)(char c);

void hal_keyboard_init(hal_keyboard_char_cb_t cb);

#endif
