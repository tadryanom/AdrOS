// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef VGA_CONSOLE_H
#define VGA_CONSOLE_H

#include <stdint.h>

void vga_init(void);
void vga_put_char(char c);
void vga_print(const char* str);
void vga_set_color(uint8_t fg, uint8_t bg);
void vga_clear(void);

#endif
