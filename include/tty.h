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

struct termios {
    uint32_t c_lflag;
};

enum {
    TTY_ICANON = 0x0001,
    TTY_ECHO   = 0x0002,
};

void tty_init(void);

int tty_read(void* user_buf, uint32_t len);
int tty_write(const void* user_buf, uint32_t len);

int tty_read_kbuf(void* kbuf, uint32_t len);
int tty_write_kbuf(const void* kbuf, uint32_t len);

int tty_can_read(void);
int tty_can_write(void);

int tty_ioctl(uint32_t cmd, void* user_arg);

void tty_input_char(char c);

#endif
