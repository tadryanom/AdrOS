// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef ULIBC_LINUX_FUTEX_H
#define ULIBC_LINUX_FUTEX_H

#include <stdint.h>
#include <sys/time.h>

#define FUTEX_WAIT 0
#define FUTEX_WAKE 1

int futex(uint32_t* uaddr, int op, uint32_t val, const struct timeval* timeout);

#endif
