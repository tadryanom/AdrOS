// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#ifndef UART_CONSOLE_H
#define UART_CONSOLE_H

void uart_init(void);
void uart_put_char(char c);
void uart_print(const char* str);

#endif
