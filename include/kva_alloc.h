// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#ifndef KVA_ALLOC_H
#define KVA_ALLOC_H

#include <stdint.h>

/*
 * Kernel Virtual Address Allocator
 *
 * Dynamically allocates virtual address ranges for MMIO/DMA/temporary mappings.
 * Replaces fixed VAs like KVA_PHYS_MAP and VIRTIO_VRING_VA.
 *
 * Allocator region: 0xC0500000 .. 0xC0800000 (3 MB, 768 pages)
 * - Below 0xC0500000: Fixed MMIO regions (ACPI, ATA, E1000, LAPIC, IOAPIC)
 * - Above 0xC0800000: Kernel stacks, heap, initrd, framebuffer
 *
 * NOTE: The allocator region (0xC0500000..0xC0800000) falls within the initial
 * 16MB linear mapping set up by boot.S (0xC0000000..0xC0100000). This means
 * V2P() works by coincidence for addresses in this range, but code should use
 * vmm_virt_to_phys() instead to be portable and future-proof.
 */

/* Initialize the KVA allocator */
void kva_alloc_init(void);

/* Allocate a contiguous range of pages (returns 0 on failure) */
uintptr_t kva_alloc_pages(uint32_t page_count);

/* Free a previously allocated range */
void kva_free_pages(uintptr_t virt, uint32_t page_count);

#endif
