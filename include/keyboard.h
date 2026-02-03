// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

// Callback function type: called when a char is decoded
typedef void (*keyboard_callback_t)(char);

void keyboard_init(void);
void keyboard_set_callback(keyboard_callback_t callback);

#endif
