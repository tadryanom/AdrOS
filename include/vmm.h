#ifndef VMM_H
#define VMM_H

#include <stdint.h>

/* Page Flags */
#define VMM_FLAG_PRESENT  (1 << 0)
#define VMM_FLAG_RW       (1 << 1)
#define VMM_FLAG_USER     (1 << 2)

/* 
 * Initialize Virtual Memory Manager
 * Should set up the kernel page table/directory and enable paging.
 */
void vmm_init(void);

/*
 * Map a physical page to a virtual address.
 * phys: Physical address (must be page aligned)
 * virt: Virtual address (must be page aligned)
 * flags: Permission flags
 */
void vmm_map_page(uint64_t phys, uint64_t virt, uint32_t flags);

/*
 * Update flags for an already-mapped virtual page.
 * Keeps the physical frame, only changes PRESENT/RW/USER bits.
 */
void vmm_set_page_flags(uint64_t virt, uint32_t flags);

/*
 * Update flags for an already-mapped virtual range.
 * vaddr/len may be unaligned.
 */
void vmm_protect_range(uint64_t vaddr, uint64_t len, uint32_t flags);

/*
 * Unmap a virtual page.
 */
void vmm_unmap_page(uint64_t virt);

#endif
