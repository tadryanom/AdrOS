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
#define X86_PTE_COW     0x200  /* Bit 9: OS-available, marks Copy-on-Write */

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
    if (flags & VMM_FLAG_COW)     x86_flags |= X86_PTE_COW;
    return x86_flags;
}

static volatile uint32_t* x86_pd_recursive(void) {
    return (volatile uint32_t*)0xFFFFF000U;
}

static volatile uint32_t* x86_pt_recursive(uint32_t pd_index) {
    return (volatile uint32_t*)0xFFC00000U + ((uintptr_t)pd_index << 10);
}

static const volatile uint32_t* vmm_active_pd_virt(void) {
    return x86_pd_recursive();
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

void vmm_map_page(uint64_t phys, uint64_t virt, uint32_t flags) {
    uint32_t pd_index = (uint32_t)(virt >> 22);
    uint32_t pt_index = (uint32_t)((virt >> 12) & 0x03FF);

    volatile uint32_t* pd = x86_pd_recursive();
    if ((pd[pd_index] & X86_PTE_PRESENT) == 0) {
        uint32_t pt_phys = (uint32_t)pmm_alloc_page_low();
        if (!pt_phys) {
            uart_print("[VMM] OOM allocating page table.\n");
            return;
        }

        uint32_t pde_flags = X86_PTE_PRESENT | X86_PTE_RW;
        if (flags & VMM_FLAG_USER) pde_flags |= X86_PTE_USER;
        pd[pd_index] = pt_phys | pde_flags;

        // Make sure the page-table window reflects the new PDE before touching it.
        invlpg((uintptr_t)x86_pt_recursive(pd_index));

        volatile uint32_t* pt = x86_pt_recursive(pd_index);
        for (int i = 0; i < 1024; i++) pt[i] = 0;
    }

    if ((flags & VMM_FLAG_USER) && ((pd[pd_index] & X86_PTE_USER) == 0)) {
        pd[pd_index] |= X86_PTE_USER;
    }

    volatile uint32_t* pt = x86_pt_recursive(pd_index);
    pt[pt_index] = ((uint32_t)phys) | vmm_flags_to_x86(flags);
    invlpg((uintptr_t)virt);
}

uintptr_t vmm_as_create_kernel_clone(void) {
    uint32_t pd_phys = (uint32_t)pmm_alloc_page_low();
    if (!pd_phys) return 0;

    // Initialize the new page directory by temporarily mapping it into the current address
    // space. We avoid assuming any global phys->virt linear mapping exists.
    const uint64_t TMP_PD_VA = 0xBFFFE000ULL;
    vmm_map_page((uint64_t)pd_phys, TMP_PD_VA, VMM_FLAG_PRESENT | VMM_FLAG_RW);
    uint32_t* pd_tmp = (uint32_t*)(uintptr_t)TMP_PD_VA;
    for (int i = 0; i < 1024; i++) pd_tmp[i] = 0;

    // Copy current kernel mappings (higher-half PDEs). This must include dynamic mappings
    // created after boot (e.g. initrd physical range mapping).
    const volatile uint32_t* active_pd = vmm_active_pd_virt();
    for (int i = 768; i < 1024; i++) {
        pd_tmp[i] = (uint32_t)active_pd[i];
    }

    // Fix recursive mapping: PDE[1023] must point to this PD.
    pd_tmp[1023] = pd_phys | X86_PTE_PRESENT | X86_PTE_RW;

    vmm_unmap_page(TMP_PD_VA);
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

    // Best-effort clone: copy present user mappings (USER PTEs), ignore kernel half.
    uintptr_t old_as = hal_cpu_get_address_space();
    vmm_as_activate(src_as);
    const volatile uint32_t* src_pd = x86_pd_recursive();

    for (uint32_t pdi = 0; pdi < 768; pdi++) {
        uint32_t pde = (uint32_t)src_pd[pdi];
        if ((pde & X86_PTE_PRESENT) == 0) continue;

        const volatile uint32_t* src_pt = x86_pt_recursive(pdi);

        for (uint32_t pti = 0; pti < 1024; pti++) {
            uint32_t pte = (uint32_t)src_pt[pti];
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
            // src_as is active here
            vmm_map_page((uint64_t)src_frame, (uint64_t)TMP_MAP_VA, VMM_FLAG_PRESENT | VMM_FLAG_RW);
            memcpy(tmp, (const void*)TMP_MAP_VA, 4096);
            vmm_unmap_page((uint64_t)TMP_MAP_VA);

            vmm_as_activate(new_as);
            vmm_map_page((uint64_t)(uintptr_t)dst_frame, (uint64_t)TMP_MAP_VA, VMM_FLAG_PRESENT | VMM_FLAG_RW);
            memcpy((void*)TMP_MAP_VA, tmp, 4096);
            vmm_unmap_page((uint64_t)TMP_MAP_VA);

            vmm_as_activate(src_as);

        }
    }

    vmm_as_activate(old_as);

    kfree(tmp);
    return new_as;
}

void vmm_as_activate(uintptr_t as) {
    if (!as) return;
    hal_cpu_set_address_space(as);
}

void vmm_as_map_page(uintptr_t as, uint64_t phys, uint64_t virt, uint32_t flags) {
    if (!as) return;
    uintptr_t old_as = hal_cpu_get_address_space();
    if ((old_as & ~(uintptr_t)0xFFFU) != (as & ~(uintptr_t)0xFFFU)) {
        vmm_as_activate(as);
        vmm_map_page(phys, virt, flags);
        vmm_as_activate(old_as);
    } else {
        vmm_map_page(phys, virt, flags);
    }
}

void vmm_as_destroy(uintptr_t as) {
    if (!as) return;
    if (as == g_kernel_as) return;

    uintptr_t old_as = hal_cpu_get_address_space();
    vmm_as_activate(as);
    volatile uint32_t* pd = x86_pd_recursive();

    // Free user page tables + frames for user space.
    for (int pdi = 0; pdi < 768; pdi++) {
        uint32_t pde = (uint32_t)pd[pdi];
        if ((pde & X86_PTE_PRESENT) == 0) continue;

        uint32_t pt_phys = pde & 0xFFFFF000;
        volatile uint32_t* pt = x86_pt_recursive((uint32_t)pdi);

        for (int pti = 0; pti < 1024; pti++) {
            uint32_t pte = (uint32_t)pt[pti];
            if ((pte & X86_PTE_PRESENT) == 0) continue;
            uint32_t frame = pte & 0xFFFFF000;
            pmm_free_page((void*)(uintptr_t)frame);
            pt[pti] = 0;
        }

        pmm_free_page((void*)(uintptr_t)pt_phys);
        pd[pdi] = 0;
    }

    vmm_as_activate(old_as);
    pmm_free_page((void*)(uintptr_t)as);
}

void vmm_set_page_flags(uint64_t virt, uint32_t flags) {
    uint32_t pd_index = virt >> 22;
    uint32_t pt_index = (virt >> 12) & 0x03FF;

    volatile uint32_t* pd = x86_pd_recursive();
    if ((pd[pd_index] & X86_PTE_PRESENT) == 0) return;

    volatile uint32_t* pt = x86_pt_recursive(pd_index);
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

    volatile uint32_t* pd = x86_pd_recursive();
    if ((pd[pd_index] & X86_PTE_PRESENT) == 0) return;
    volatile uint32_t* pt = x86_pt_recursive(pd_index);
    pt[pt_index] = 0;
    invlpg((uintptr_t)virt);
}

uintptr_t vmm_as_clone_user_cow(uintptr_t src_as) {
    if (!src_as) return 0;

    uintptr_t new_as = vmm_as_create_kernel_clone();
    if (!new_as) return 0;

    uintptr_t old_as = hal_cpu_get_address_space();
    vmm_as_activate(src_as);
    volatile uint32_t* src_pd = x86_pd_recursive();

    for (uint32_t pdi = 0; pdi < 768; pdi++) {
        uint32_t pde = (uint32_t)src_pd[pdi];
        if ((pde & X86_PTE_PRESENT) == 0) continue;

        volatile uint32_t* src_pt = x86_pt_recursive(pdi);

        for (uint32_t pti = 0; pti < 1024; pti++) {
            uint32_t pte = (uint32_t)src_pt[pti];
            if (!(pte & X86_PTE_PRESENT)) continue;
            if ((pte & X86_PTE_USER) == 0) continue;

            uint32_t frame_phys = pte & 0xFFFFF000;
            uintptr_t va = ((uintptr_t)pdi << 22) | ((uintptr_t)pti << 12);

            // Mark source page as read-only + CoW if it was writable.
            uint32_t new_pte = frame_phys | X86_PTE_PRESENT | X86_PTE_USER;
            if (pte & X86_PTE_RW) {
                new_pte |= X86_PTE_COW;  // Was writable -> CoW
                // Remove RW from source
                src_pt[pti] = new_pte;
                invlpg(va);
            } else {
                new_pte = pte;  // Keep as-is (read-only text, etc.)
            }

            // Increment physical frame refcount
            pmm_incref((uintptr_t)frame_phys);

            // Map same frame into child with same flags
            vmm_as_map_page(new_as, (uint64_t)frame_phys, (uint64_t)va,
                            VMM_FLAG_PRESENT | VMM_FLAG_USER |
                            ((new_pte & X86_PTE_COW) ? VMM_FLAG_COW : 0));
        }
    }

    vmm_as_activate(old_as);
    return new_as;
}

int vmm_handle_cow_fault(uintptr_t fault_addr) {
    uintptr_t va = fault_addr & ~(uintptr_t)0xFFF;
    uint32_t pdi = va >> 22;
    uint32_t pti = (va >> 12) & 0x3FF;

    if (pdi >= 768) return 0;  // Kernel space, not CoW

    volatile uint32_t* pd = x86_pd_recursive();
    if ((pd[pdi] & X86_PTE_PRESENT) == 0) return 0;

    volatile uint32_t* pt = x86_pt_recursive(pdi);
    uint32_t pte = pt[pti];

    if (!(pte & X86_PTE_PRESENT)) return 0;
    if (!(pte & X86_PTE_COW)) return 0;

    uint32_t old_frame = pte & 0xFFFFF000;
    uint16_t rc = pmm_get_refcount((uintptr_t)old_frame);

    if (rc <= 1) {
        // We're the sole owner — just make it writable and clear CoW.
        pt[pti] = old_frame | X86_PTE_PRESENT | X86_PTE_RW | X86_PTE_USER;
        invlpg(va);
        return 1;
    }

    // Allocate a new frame and copy the page contents.
    void* new_frame = pmm_alloc_page();
    if (!new_frame) return 0;  // OOM — caller will SIGSEGV

    // Use a temporary kernel VA to copy data.
    const uintptr_t TMP_COW_VA = 0xBFFFD000U;
    vmm_map_page((uint64_t)(uintptr_t)new_frame, (uint64_t)TMP_COW_VA,
                 VMM_FLAG_PRESENT | VMM_FLAG_RW);
    memcpy((void*)TMP_COW_VA, (const void*)va, 4096);
    vmm_unmap_page((uint64_t)TMP_COW_VA);

    // Decrement old frame refcount.
    pmm_decref((uintptr_t)old_frame);

    // Map new frame as writable (no CoW).
    pt[pti] = (uint32_t)(uintptr_t)new_frame | X86_PTE_PRESENT | X86_PTE_RW | X86_PTE_USER;
    invlpg(va);

    return 1;
}

void vmm_init(void) {
    uart_print("[VMM] Higher Half Kernel Active.\n");

    g_kernel_as = hal_cpu_get_address_space();
    
    // Test mapping
    vmm_map_page(0xB8000, 0xC00B8000, VMM_FLAG_PRESENT | VMM_FLAG_RW);
    uart_print("[VMM] Mapped VGA to 0xC00B8000.\n");
}
