// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#ifndef HEAP_H
#define HEAP_H

#include <stddef.h>
#include <stdint.h>

void kheap_init(void);
void* kmalloc(size_t size);
void kfree(void* ptr);

#endif
