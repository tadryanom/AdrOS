// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef CONSOLE_H
#define CONSOLE_H

#include <stddef.h>
#include <stdarg.h>

void console_init(void);
void console_enable_uart(int enabled);
void console_enable_vga(int enabled);

void console_write(const char* s);
void console_put_char(char c);

int kvsnprintf(char* out, size_t out_size, const char* fmt, va_list ap);
int ksnprintf(char* out, size_t out_size, const char* fmt, ...);

void kprintf(const char* fmt, ...);

int kgetc(void);

void klog_set_suppress(int suppress);
size_t klog_read(char* out, size_t out_size);

#endif
