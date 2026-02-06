#include "vmm.h"
#include "pmm.h"
#include "uart_console.h"
#include <stddef.h>

/* Constants */
#define KERNEL_VIRT_BASE 0xC0000000
#define PAGE_SIZE 4096

/* Macros for address translation */
#define V2P(x) ((uintptr_t)(x) - KERNEL_VIRT_BASE)
#define P2V(x) ((uintptr_t)(x) + KERNEL_VIRT_BASE)

/* x86 Paging Flags */
#define X86_PTE_PRESENT 0x1
#define X86_PTE_RW      0x2
#define X86_PTE_USER    0x4

/* Defined in boot.S (Physical address loaded in CR3, but accessed via virt alias) */
/* Wait, boot_pd is in BSS. Linker put it at 0xC0xxxxxx. 
   So accessing boot_pd directly works fine! */
extern uint32_t boot_pd[1024];

static void* pmm_alloc_page_low(void) {
    // Bring-up safety: allocate only from identity-mapped area (0-4MB)
    // until we have a general phys->virt mapping.
    for (int tries = 0; tries < 1024; tries++) {
        void* p = pmm_alloc_page();
        if (!p) return 0;
        if ((uintptr_t)p < 0x01000000) {
            return p;
        }
        pmm_free_page(p);
    }
    return 0;
}

static inline void invlpg(uintptr_t vaddr) {
    __asm__ volatile("invlpg (%0)" : : "r" (vaddr) : "memory");
}

void vmm_map_page(uint64_t phys, uint64_t virt, uint32_t flags) {
    uint32_t pd_index = virt >> 22;
    uint32_t pt_index = (virt >> 12) & 0x03FF;

    // Check if Page Table exists
    if (!(boot_pd[pd_index] & X86_PTE_PRESENT)) {
        // Allocate a new PT
        uint32_t pt_phys = (uint32_t)pmm_alloc_page_low();
        if (!pt_phys) {
            uart_print("[VMM] OOM allocating page table.\n");
            return;
        }

        // ACCESS SAFETY: Convert Physical to Virtual to write to it
        uint32_t* pt_virt = (uint32_t*)P2V(pt_phys);
        
        // Clear table
        for(int i=0; i<1024; i++) pt_virt[i] = 0;

        // Add to Directory
        uint32_t pde_flags = X86_PTE_PRESENT | X86_PTE_RW;
        if (flags & VMM_FLAG_USER) pde_flags |= X86_PTE_USER;
        boot_pd[pd_index] = pt_phys | pde_flags;
    }

    if ((flags & VMM_FLAG_USER) && !(boot_pd[pd_index] & X86_PTE_USER)) {
        boot_pd[pd_index] |= X86_PTE_USER;
    }

    // Get table address from Directory
    uint32_t pt_phys = boot_pd[pd_index] & 0xFFFFF000;
    
    // ACCESS SAFETY: Convert to Virtual
    uint32_t* pt = (uint32_t*)P2V(pt_phys);
    
    uint32_t x86_flags = 0;
    if (flags & VMM_FLAG_PRESENT) x86_flags |= X86_PTE_PRESENT;
    if (flags & VMM_FLAG_RW)      x86_flags |= X86_PTE_RW;
    if (flags & VMM_FLAG_USER)    x86_flags |= X86_PTE_USER;
    
    pt[pt_index] = ((uint32_t)phys) | x86_flags;
    invlpg(virt);
}

void vmm_unmap_page(uint64_t virt) {
    uint32_t pd_index = virt >> 22;
    uint32_t pt_index = (virt >> 12) & 0x03FF;
    
    if (boot_pd[pd_index] & X86_PTE_PRESENT) {
        uint32_t pt_phys = boot_pd[pd_index] & 0xFFFFF000;
        uint32_t* pt = (uint32_t*)P2V(pt_phys);
        
        pt[pt_index] = 0;
        invlpg(virt);
    }
}

void vmm_init(void) {
    uart_print("[VMM] Higher Half Kernel Active.\n");
    
    // Test mapping
    vmm_map_page(0xB8000, 0xC00B8000, VMM_FLAG_PRESENT | VMM_FLAG_RW);
    uart_print("[VMM] Mapped VGA to 0xC00B8000.\n");
}
