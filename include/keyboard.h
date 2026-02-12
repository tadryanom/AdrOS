// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

// Callback function type: called when a char is decoded
typedef void (*keyboard_callback_t)(char);

void keyboard_init(void);
void keyboard_register_devfs(void);
void keyboard_set_callback(keyboard_callback_t callback);

int keyboard_read_nonblock(char* out, uint32_t max_len);

int keyboard_read_blocking(char* out, uint32_t max_len);

#endif
