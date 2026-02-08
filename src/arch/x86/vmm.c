#include "vmm.h"
#include "pmm.h"
#include "heap.h"
#include "uart_console.h"
#include "utils.h"
#include "hal/cpu.h"
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

static uintptr_t g_kernel_as = 0;

static inline void invlpg(uintptr_t vaddr) {
    __asm__ volatile("invlpg (%0)" : : "r" (vaddr) : "memory");
}

static uint32_t vmm_flags_to_x86(uint32_t flags) {
    uint32_t x86_flags = 0;
    if (flags & VMM_FLAG_PRESENT) x86_flags |= X86_PTE_PRESENT;
    if (flags & VMM_FLAG_RW)      x86_flags |= X86_PTE_RW;
    if (flags & VMM_FLAG_USER)    x86_flags |= X86_PTE_USER;
    return x86_flags;
}

static uint32_t* vmm_active_pd_virt(void) {
    uintptr_t as = hal_cpu_get_address_space();
    return (uint32_t*)P2V((uint32_t)as);
}

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

static void vmm_map_page_in_pd(uint32_t* pd_virt, uint64_t phys, uint64_t virt, uint32_t flags) {
    uint32_t pd_index = virt >> 22;
    uint32_t pt_index = (virt >> 12) & 0x03FF;

    if (!(pd_virt[pd_index] & X86_PTE_PRESENT)) {
        uint32_t pt_phys = (uint32_t)pmm_alloc_page_low();
        if (!pt_phys) {
            uart_print("[VMM] OOM allocating page table.\n");
            return;
        }

        uint32_t* pt_virt = (uint32_t*)P2V(pt_phys);
        for (int i = 0; i < 1024; i++) pt_virt[i] = 0;

        uint32_t pde_flags = X86_PTE_PRESENT | X86_PTE_RW;
        if (flags & VMM_FLAG_USER) pde_flags |= X86_PTE_USER;
        pd_virt[pd_index] = pt_phys | pde_flags;
    }

    if ((flags & VMM_FLAG_USER) && !(pd_virt[pd_index] & X86_PTE_USER)) {
        pd_virt[pd_index] |= X86_PTE_USER;
    }

    uint32_t pt_phys = pd_virt[pd_index] & 0xFFFFF000;
    uint32_t* pt = (uint32_t*)P2V(pt_phys);
    pt[pt_index] = ((uint32_t)phys) | vmm_flags_to_x86(flags);
    invlpg((uintptr_t)virt);
}

void vmm_map_page(uint64_t phys, uint64_t virt, uint32_t flags) {
    vmm_map_page_in_pd(vmm_active_pd_virt(), phys, virt, flags);
}

uintptr_t vmm_as_create_kernel_clone(void) {
    uint32_t pd_phys = (uint32_t)pmm_alloc_page_low();
    if (!pd_phys) return 0;

    uint32_t* pd_virt = (uint32_t*)P2V(pd_phys);
    for (int i = 0; i < 1024; i++) pd_virt[i] = 0;

    // Copy kernel mappings (higher-half PDEs)
    for (int i = 768; i < 1024; i++) {
        pd_virt[i] = boot_pd[i];
    }

    // Fix recursive mapping: PDE[1023] must point to this PD, not boot_pd.
    pd_virt[1023] = pd_phys | X86_PTE_PRESENT | X86_PTE_RW;

    return (uintptr_t)pd_phys;
}

uintptr_t vmm_as_clone_user(uintptr_t src_as) {
    if (!src_as) return 0;

    // Temporary kernel-only mapping in the last user PDE (pdi=767). This avoids touching
    // shared higher-half kernel page tables copied from boot_pd.
    const uintptr_t TMP_MAP_VA = 0xBFF00000U;

    uintptr_t new_as = vmm_as_create_kernel_clone();
    if (!new_as) return 0;

    uint8_t* tmp = (uint8_t*)kmalloc(4096);
    if (!tmp) {
        vmm_as_destroy(new_as);
        return 0;
    }

    const uint32_t* src_pd = (const uint32_t*)P2V((uint32_t)src_as);

    // Best-effort clone: copy present user mappings (USER PTEs), ignore kernel half.
    for (uint32_t pdi = 0; pdi < 768; pdi++) {
        uint32_t pde = src_pd[pdi];
        if (!(pde & X86_PTE_PRESENT)) continue;

        uint32_t src_pt_phys = pde & 0xFFFFF000;
        const uint32_t* src_pt = (const uint32_t*)P2V(src_pt_phys);

        for (uint32_t pti = 0; pti < 1024; pti++) {
            uint32_t pte = src_pt[pti];
            if (!(pte & X86_PTE_PRESENT)) continue;
            if ((pte & X86_PTE_USER) == 0) continue;
            const uint32_t x86_flags = pte & 0xFFF;

            // Derive VMM flags.
            uint32_t flags = VMM_FLAG_PRESENT;
            if (x86_flags & X86_PTE_RW) flags |= VMM_FLAG_RW;
            if (x86_flags & X86_PTE_USER) flags |= VMM_FLAG_USER;

            void* dst_frame = pmm_alloc_page_low();
            if (!dst_frame) {
                vmm_as_destroy(new_as);
                return 0;
            }

            uint32_t src_frame = pte & 0xFFFFF000;

            uintptr_t va = ((uintptr_t)pdi << 22) | ((uintptr_t)pti << 12);
            vmm_as_map_page(new_as, (uint64_t)(uintptr_t)dst_frame, (uint64_t)va, flags);

            // Copy contents by mapping frames into a temporary kernel VA under each address space.
            uintptr_t old_as = hal_cpu_get_address_space();
            vmm_as_activate(src_as);
            vmm_map_page((uint64_t)src_frame, (uint64_t)TMP_MAP_VA, VMM_FLAG_PRESENT | VMM_FLAG_RW);
            memcpy(tmp, (const void*)TMP_MAP_VA, 4096);
            vmm_unmap_page((uint64_t)TMP_MAP_VA);

            vmm_as_activate(new_as);
            vmm_map_page((uint64_t)(uintptr_t)dst_frame, (uint64_t)TMP_MAP_VA, VMM_FLAG_PRESENT | VMM_FLAG_RW);
            memcpy((void*)TMP_MAP_VA, tmp, 4096);
            vmm_unmap_page((uint64_t)TMP_MAP_VA);
            vmm_as_activate(old_as);

        }
    }

    kfree(tmp);
    return new_as;
}

void vmm_as_activate(uintptr_t as) {
    if (!as) return;
    hal_cpu_set_address_space(as);
}

void vmm_as_map_page(uintptr_t as, uint64_t phys, uint64_t virt, uint32_t flags) {
    if (!as) return;
    uint32_t* pd_virt = (uint32_t*)P2V((uint32_t)as);
    vmm_map_page_in_pd(pd_virt, phys, virt, flags);
}

void vmm_as_destroy(uintptr_t as) {
    if (!as) return;
    if (as == g_kernel_as) return;

    uint32_t* pd = (uint32_t*)P2V((uint32_t)as);

    // Free user page tables + frames for user space.
    for (int pdi = 0; pdi < 768; pdi++) {
        uint32_t pde = pd[pdi];
        if (!(pde & X86_PTE_PRESENT)) continue;

        uint32_t pt_phys = pde & 0xFFFFF000;
        uint32_t* pt = (uint32_t*)P2V(pt_phys);

        for (int pti = 0; pti < 1024; pti++) {
            uint32_t pte = pt[pti];
            if (!(pte & X86_PTE_PRESENT)) continue;
            uint32_t frame = pte & 0xFFFFF000;
            pmm_free_page((void*)(uintptr_t)frame);
            pt[pti] = 0;
        }

        pmm_free_page((void*)(uintptr_t)pt_phys);
        pd[pdi] = 0;
    }

    pmm_free_page((void*)(uintptr_t)as);
}

void vmm_set_page_flags(uint64_t virt, uint32_t flags) {
    uint32_t pd_index = virt >> 22;
    uint32_t pt_index = (virt >> 12) & 0x03FF;

    const uint32_t* pd = vmm_active_pd_virt();

    if (!(pd[pd_index] & X86_PTE_PRESENT)) {
        return;
    }

    uint32_t pt_phys = pd[pd_index] & 0xFFFFF000;
    uint32_t* pt = (uint32_t*)P2V(pt_phys);

    uint32_t pte = pt[pt_index];
    if (!(pte & X86_PTE_PRESENT)) {
        return;
    }

    uint32_t phys = pte & 0xFFFFF000;
    pt[pt_index] = phys | vmm_flags_to_x86(flags);
    invlpg((uintptr_t)virt);
}

void vmm_protect_range(uint64_t vaddr, uint64_t len, uint32_t flags) {
    if (len == 0) return;

    uint64_t start = vaddr & ~0xFFFULL;
    uint64_t end = (vaddr + len - 1) & ~0xFFFULL;
    for (uint64_t va = start;; va += 0x1000ULL) {
        vmm_set_page_flags(va, flags | VMM_FLAG_PRESENT);
        if (va == end) break;
    }
}

void vmm_unmap_page(uint64_t virt) {
    uint32_t pd_index = virt >> 22;
    uint32_t pt_index = (virt >> 12) & 0x03FF;

    const uint32_t* pd = vmm_active_pd_virt();
    if (pd[pd_index] & X86_PTE_PRESENT) {
        uint32_t pt_phys = pd[pd_index] & 0xFFFFF000;
        uint32_t* pt = (uint32_t*)P2V(pt_phys);
        
        pt[pt_index] = 0;
        invlpg(virt);
    }
}

void vmm_init(void) {
    uart_print("[VMM] Higher Half Kernel Active.\n");

    g_kernel_as = hal_cpu_get_address_space();
    
    // Test mapping
    vmm_map_page(0xB8000, 0xC00B8000, VMM_FLAG_PRESENT | VMM_FLAG_RW);
    uart_print("[VMM] Mapped VGA to 0xC00B8000.\n");
}
