// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef ULIBC_STDIO_H
#define ULIBC_STDIO_H

#include <stddef.h>
#include <stdarg.h>

int     putchar(int c);
int     puts(const char* s);
int     printf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
int     vprintf(const char* fmt, va_list ap);
int     snprintf(char* buf, size_t size, const char* fmt, ...) __attribute__((format(printf, 3, 4)));
int     vsnprintf(char* buf, size_t size, const char* fmt, va_list ap);

#endif
