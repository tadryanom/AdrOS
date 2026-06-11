// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#include "kva_alloc.h"

#include "console.h"
#include "hal/cpu.h"
#include "spinlock.h"

#include <stddef.h>
#include <string.h>

/* Allocator region: 0xC0500000 .. 0xC0800000 (3 MB, 768 pages) */
#define KVA_ALLOC_BASE     0xC0500000U
#define KVA_ALLOC_END      0xC0800000U
#define KVA_ALLOC_PAGES    ((KVA_ALLOC_END - KVA_ALLOC_BASE) >> 12)

/* Bitmap for allocation tracking (1 bit per page) */
static uint8_t g_kva_bitmap[KVA_ALLOC_PAGES / 8];
static spinlock_t g_kva_lock;

void kva_alloc_init(void) {
    memset(g_kva_bitmap, 0, sizeof(g_kva_bitmap));
    spinlock_init(&g_kva_lock);
}

static inline void bitmap_set(uint32_t bit) {
    g_kva_bitmap[bit >> 3] |= (1 << (bit & 7));
}

static inline void bitmap_clear(uint32_t bit) {
    g_kva_bitmap[bit >> 3] &= ~(1 << (bit & 7));
}

static inline int bitmap_test(uint32_t bit) {
    return (g_kva_bitmap[bit >> 3] >> (bit & 7)) & 1;
}

uintptr_t kva_alloc_pages(uint32_t page_count) {
    if (page_count == 0 || page_count > KVA_ALLOC_PAGES) {
        return 0;
    }

    spin_lock(&g_kva_lock);

    /* Find contiguous free range */
    uint32_t start = 0;
    uint32_t found = 0;
    for (uint32_t i = 0; i <= KVA_ALLOC_PAGES - page_count; i++) {
        int free = 1;
        for (uint32_t j = 0; j < page_count; j++) {
            if (bitmap_test(i + j)) {
                free = 0;
                break;
            }
        }
        if (free) {
            start = i;
            found = 1;
            break;
        }
    }

    if (!found) {
        spin_unlock(&g_kva_lock);
        return 0;
    }

    /* Mark pages as allocated */
    for (uint32_t i = 0; i < page_count; i++) {
        bitmap_set(start + i);
    }

    spin_unlock(&g_kva_lock);

    return KVA_ALLOC_BASE + (start << 12);
}

void kva_free_pages(uintptr_t virt, uint32_t page_count) {
    if (virt < KVA_ALLOC_BASE || virt >= KVA_ALLOC_END) {
        return;
    }

    uint32_t start = (virt - KVA_ALLOC_BASE) >> 12;
    if (start + page_count > KVA_ALLOC_PAGES) {
        return;
    }

    spin_lock(&g_kva_lock);

    for (uint32_t i = 0; i < page_count; i++) {
        bitmap_clear(start + i);
    }

    spin_unlock(&g_kva_lock);
}
