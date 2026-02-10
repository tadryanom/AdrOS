// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef PMM_H
#define PMM_H

#include <stdint.h>

#define PAGE_SIZE 4096

// Initialize the Physical Memory Manager
// boot_info is architecture dependent (Multiboot info on x86)
void pmm_init(void* boot_info);

// Allocate a single physical page
void* pmm_alloc_page(void);

// Free a physical page (decrements refcount, frees at 0)
void pmm_free_page(void* ptr);

// Reference counting for Copy-on-Write
void pmm_incref(uintptr_t paddr);
uint16_t pmm_decref(uintptr_t paddr);
uint16_t pmm_get_refcount(uintptr_t paddr);

// Helper to print memory stats
void pmm_print_stats(void);

#endif
