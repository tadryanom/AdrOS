// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef PTY_H
#define PTY_H

#include <stdint.h>

void pty_init(void);

int pty_master_read_kbuf(void* kbuf, uint32_t len);
int pty_master_write_kbuf(const void* kbuf, uint32_t len);

int pty_slave_read_kbuf(void* kbuf, uint32_t len);
int pty_slave_write_kbuf(const void* kbuf, uint32_t len);

int pty_master_can_read(void);
int pty_master_can_write(void);

int pty_slave_can_read(void);
int pty_slave_can_write(void);

int pty_slave_ioctl(uint32_t cmd, void* user_arg);

#endif
