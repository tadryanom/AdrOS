// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef UACCESS_H
#define UACCESS_H

#include <stddef.h>

int user_range_ok(const void* user_ptr, size_t len);
int copy_from_user(void* dst, const void* src_user, size_t len);
int copy_to_user(void* dst_user, const void* src, size_t len);

#endif
