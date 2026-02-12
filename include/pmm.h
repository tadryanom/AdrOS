#ifndef PMM_H
#define PMM_H

#include <stdint.h>

#define PAGE_SIZE 4096

// Initialize the Physical Memory Manager
// boot_info is architecture dependent (Multiboot info on x86)
void pmm_init(void* boot_info);

// Mark a range of physical memory as used (1) or free (0).
// Called by arch-specific boot code to describe the memory map.
void pmm_mark_region(uint64_t base, uint64_t size, int used);

// Set total memory size and max frame count.
// Must be called by arch boot code before marking regions.
void pmm_set_limits(uint64_t total_mem, uint64_t max_fr);

// Architecture-specific boot info parser.
// Implemented per-arch (e.g. Multiboot2 on x86, DTB on ARM).
// Called by pmm_init(). Must call pmm_set_limits() then pmm_mark_region().
void pmm_arch_init(void* boot_info);

// Allocate a single physical page
void* pmm_alloc_page(void);

// Allocate N contiguous physical pages (for DMA buffers etc.)
void* pmm_alloc_blocks(uint32_t count);

// Free N contiguous physical pages
void pmm_free_blocks(void* ptr, uint32_t count);

// Free a physical page (decrements refcount, frees at 0)
void pmm_free_page(void* ptr);

// Reference counting for Copy-on-Write
void pmm_incref(uintptr_t paddr);
uint16_t pmm_decref(uintptr_t paddr);
uint16_t pmm_get_refcount(uintptr_t paddr);

// Helper to print memory stats
void pmm_print_stats(void);

#endif
