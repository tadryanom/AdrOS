// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#ifndef CONSOLE_H
#define CONSOLE_H

#include <stddef.h>
#include <stdarg.h>

void console_init(void);
void console_enable_uart(int enabled);
void console_enable_vga(int enabled);

void console_write(const char* s);

int kvsnprintf(char* out, size_t out_size, const char* fmt, va_list ap);
int ksnprintf(char* out, size_t out_size, const char* fmt, ...);

void kprintf(const char* fmt, ...);

#endif
