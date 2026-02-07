// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef TTY_H
#define TTY_H

#include <stddef.h>
#include <stdint.h>

void tty_init(void);

int tty_read(void* user_buf, uint32_t len);
int tty_write(const void* user_buf, uint32_t len);

void tty_input_char(char c);

#endif
