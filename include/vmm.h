#ifndef VMM_H
#define VMM_H

#include <stdint.h>

/* Page Flags */
#define VMM_FLAG_PRESENT  (1 << 0)
#define VMM_FLAG_RW       (1 << 1)
#define VMM_FLAG_USER     (1 << 2)
#define VMM_FLAG_PWT      (1 << 3)  /* Page Write-Through */
#define VMM_FLAG_PCD      (1 << 4)  /* Page Cache Disable */
#define VMM_FLAG_NOCACHE  (VMM_FLAG_PWT | VMM_FLAG_PCD) /* For MMIO regions */
#define VMM_FLAG_COW      (1 << 9)  /* OS-available bit: Copy-on-Write marker */
#define VMM_FLAG_NX       (1 << 10) /* No-Execute (PAE bit 63) */

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

uintptr_t vmm_as_create_kernel_clone(void);
void vmm_as_destroy(uintptr_t as);
void vmm_as_activate(uintptr_t as);
void vmm_as_map_page(uintptr_t as, uint64_t phys, uint64_t virt, uint32_t flags);

uintptr_t vmm_as_clone_user(uintptr_t src_as);

/*
 * Clone user address space using Copy-on-Write.
 * Shared pages are marked read-only + COW bit; physical frames get incref'd.
 */
uintptr_t vmm_as_clone_user_cow(uintptr_t src_as);

/*
 * Handle a Copy-on-Write page fault.
 * Returns 1 if the fault was a CoW fault and was resolved, 0 otherwise.
 */
int vmm_handle_cow_fault(uintptr_t fault_addr);

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
