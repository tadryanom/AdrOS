#include "vmm.h"
#include "pmm.h"
#include "heap.h"
#include "console.h"
#include "utils.h"
#include "hal/cpu.h"
#include "spinlock.h"
#include <stddef.h>

/*
 * PAE Paging for x86-32.
 *
 * 3-level page tables with 64-bit entries:
 *   PDPT: 4 entries x 8 bytes = 32 bytes (in CR3)
 *   PD[0..3]: 512 entries x 8 bytes = 4KB each
 *   PT: 512 entries x 8 bytes = 4KB each
 *
 * Virtual address decomposition:
 *   bits 31:30  -> PDPT index (0-3)
 *   bits 29:21  -> PD index   (0-511)
 *   bits 20:12  -> PT index   (0-511)
 *   bits 11:0   -> page offset
 *
 * Recursive mapping (set up in boot.S):
 *   PD[3][508] -> PD[0]    PD[3][509] -> PD[1]
 *   PD[3][510] -> PD[2]    PD[3][511] -> PD[3]
 *
 *   Access page table [pdpt_i][pd_i]:
 *     VA = 0xFF800000 + pdpt_i * 0x200000 + pd_i * 0x1000
 *
 *   Access page directory [pdpt_i]:
 *     VA = 0xFFFFC000 + pdpt_i * 0x1000
 */

#define KERNEL_VIRT_BASE 0xC0000000U
#define PAGE_SIZE 4096

#define V2P(x) ((uintptr_t)(x) - KERNEL_VIRT_BASE)
#define P2V(x) ((uintptr_t)(x) + KERNEL_VIRT_BASE)

/* PAE PTE/PDE low-32 flags (same bit positions as legacy) */
#define X86_PTE_PRESENT 0x1ULL
#define X86_PTE_RW      0x2ULL
#define X86_PTE_USER    0x4ULL
#define X86_PTE_PWT     0x8ULL
#define X86_PTE_PCD     0x10ULL
#define X86_PTE_COW     0x200ULL  /* Bit 9: OS-available, marks Copy-on-Write */

/* NX bit (bit 63, only effective if IA32_EFER.NXE = 1) */
#define X86_PTE_NX      (1ULL << 63)

/* Defined in boot.S */
extern uint64_t boot_pdpt[4];
extern uint64_t boot_pd0[512];
extern uint64_t boot_pd1[512];
extern uint64_t boot_pd2[512];
extern uint64_t boot_pd3[512];

static uintptr_t g_kernel_as = 0;
static spinlock_t vmm_lock = {0};

static inline void invlpg(uintptr_t vaddr) {
    __asm__ volatile("invlpg (%0)" : : "r" (vaddr) : "memory");
}

/* --- PAE address decomposition --- */

static inline uint32_t pae_pdpt_index(uint64_t va) {
    return (uint32_t)((va >> 30) & 0x3);
}

static inline uint32_t pae_pd_index(uint64_t va) {
    return (uint32_t)((va >> 21) & 0x1FF);
}

static inline uint32_t pae_pt_index(uint64_t va) {
    return (uint32_t)((va >> 12) & 0x1FF);
}

/* --- Recursive mapping accessors --- */

static volatile uint64_t* pae_pd_recursive(uint32_t pdpt_i) {
    return (volatile uint64_t*)(uintptr_t)(0xFFFFC000U + pdpt_i * 0x1000U);
}

static volatile uint64_t* pae_pt_recursive(uint32_t pdpt_i, uint32_t pd_i) {
    return (volatile uint64_t*)(uintptr_t)(0xFF800000U + pdpt_i * 0x200000U + pd_i * 0x1000U);
}

/* --- Flag conversion --- */

static uint64_t vmm_flags_to_x86(uint32_t flags) {
    uint64_t x86_flags = 0;
    if (flags & VMM_FLAG_PRESENT) x86_flags |= X86_PTE_PRESENT;
    if (flags & VMM_FLAG_RW)      x86_flags |= X86_PTE_RW;
    if (flags & VMM_FLAG_USER)    x86_flags |= X86_PTE_USER;
    if (flags & VMM_FLAG_PWT)     x86_flags |= X86_PTE_PWT;
    if (flags & VMM_FLAG_PCD)     x86_flags |= X86_PTE_PCD;
    if (flags & VMM_FLAG_COW)     x86_flags |= X86_PTE_COW;
    if (flags & VMM_FLAG_NX)      x86_flags |= X86_PTE_NX;
    return x86_flags;
}

/* User space covers PDPT indices 0-2 (0x00000000 - 0xBFFFFFFF).
 * PDPT[3] is kernel (0xC0000000 - 0xFFFFFFFF). */
#define PAE_USER_PDPT_MAX 3

/* --- Internal _nolock helpers (caller must hold vmm_lock) --- */

static void vmm_map_page_nolock(uint64_t phys, uint64_t virt, uint32_t flags) {
    uint32_t pi = pae_pdpt_index(virt);
    uint32_t di = pae_pd_index(virt);
    uint32_t ti = pae_pt_index(virt);

    volatile uint64_t* pd = pae_pd_recursive(pi);
    if ((pd[di] & X86_PTE_PRESENT) == 0) {
        uint32_t pt_phys = (uint32_t)(uintptr_t)pmm_alloc_page();
        if (!pt_phys) {
            kprintf("[VMM] OOM allocating page table.\n");
            return;
        }

        uint64_t pde_flags = X86_PTE_PRESENT | X86_PTE_RW;
        if (flags & VMM_FLAG_USER) pde_flags |= X86_PTE_USER;
        pd[di] = (uint64_t)pt_phys | pde_flags;

        invlpg((uintptr_t)pae_pt_recursive(pi, di));

        volatile uint64_t* pt = pae_pt_recursive(pi, di);
        for (int i = 0; i < 512; i++) pt[i] = 0;
    }

    if ((flags & VMM_FLAG_USER) && ((pd[di] & X86_PTE_USER) == 0)) {
        pd[di] |= X86_PTE_USER;
    }

    volatile uint64_t* pt = pae_pt_recursive(pi, di);
    pt[ti] = (phys & 0xFFFFF000ULL) | vmm_flags_to_x86(flags);
    invlpg((uintptr_t)(uint32_t)virt);
}

static void vmm_unmap_page_nolock(uint64_t virt) {
    uint32_t pi = pae_pdpt_index(virt);
    uint32_t di = pae_pd_index(virt);
    uint32_t ti = pae_pt_index(virt);

    volatile uint64_t* pd = pae_pd_recursive(pi);
    if ((pd[di] & X86_PTE_PRESENT) == 0) return;
    volatile uint64_t* pt = pae_pt_recursive(pi, di);
    pt[ti] = 0;
    invlpg((uintptr_t)(uint32_t)virt);
}

static void vmm_set_page_flags_nolock(uint64_t virt, uint32_t flags) {
    uint32_t pi = pae_pdpt_index(virt);
    uint32_t di = pae_pd_index(virt);
    uint32_t ti = pae_pt_index(virt);

    volatile uint64_t* pd = pae_pd_recursive(pi);
    if ((pd[di] & X86_PTE_PRESENT) == 0) return;

    volatile uint64_t* pt = pae_pt_recursive(pi, di);
    uint64_t pte = pt[ti];
    if (!(pte & X86_PTE_PRESENT)) return;

    uint64_t phys = pte & 0x000FFFFFFFFFF000ULL;
    pt[ti] = phys | vmm_flags_to_x86(flags);
    invlpg((uintptr_t)(uint32_t)virt);
}

static void vmm_as_map_page_nolock(uintptr_t as, uint64_t phys, uint64_t virt, uint32_t flags) {
    if (!as) return;
    uintptr_t old_as = hal_cpu_get_address_space();
    if (old_as != as) {
        vmm_as_activate(as);
        vmm_map_page_nolock(phys, virt, flags);
        vmm_as_activate(old_as);
    } else {
        vmm_map_page_nolock(phys, virt, flags);
    }
}

/* --- Core page operations (public, locking) --- */

void vmm_map_page(uint64_t phys, uint64_t virt, uint32_t flags) {
    uintptr_t irqf = spin_lock_irqsave(&vmm_lock);
    vmm_map_page_nolock(phys, virt, flags);
    spin_unlock_irqrestore(&vmm_lock, irqf);
}

void vmm_unmap_page(uint64_t virt) {
    uintptr_t irqf = spin_lock_irqsave(&vmm_lock);
    vmm_unmap_page_nolock(virt);
    spin_unlock_irqrestore(&vmm_lock, irqf);
}

void vmm_set_page_flags(uint64_t virt, uint32_t flags) {
    uintptr_t irqf = spin_lock_irqsave(&vmm_lock);
    vmm_set_page_flags_nolock(virt, flags);
    spin_unlock_irqrestore(&vmm_lock, irqf);
}

/* vmm_protect_range, vmm_as_activate, vmm_as_map_page are
 * architecture-independent and live in src/mm/vmm.c. */

/*
 * Create a new address space (PDPT + 4 PDs) that shares all kernel mappings
 * with the current address space.  User-space PDs are empty.
 *
 * Returns the *physical* address of the new PDPT (suitable for CR3).
 */
uintptr_t vmm_as_create_kernel_clone(void) {
    uintptr_t irqf = spin_lock_irqsave(&vmm_lock);

    /* Allocate PDPT (32 bytes, but occupies one page for simplicity) */
    uint32_t pdpt_phys = (uint32_t)(uintptr_t)pmm_alloc_page();
    if (!pdpt_phys) {
        spin_unlock_irqrestore(&vmm_lock, irqf);
        return 0;
    }

    /* Allocate 4 page directories */
    uint32_t pd_phys[4];
    for (int i = 0; i < 4; i++) {
        pd_phys[i] = (uint32_t)(uintptr_t)pmm_alloc_page();
        if (!pd_phys[i]) {
            for (int j = 0; j < i; j++) pmm_free_page((void*)(uintptr_t)pd_phys[j]);
            pmm_free_page((void*)(uintptr_t)pdpt_phys);
            spin_unlock_irqrestore(&vmm_lock, irqf);
            return 0;
        }
    }

    const uint64_t TMP_VA = 0xBFFFE000ULL;

    /* --- Initialize PDPT --- */
    vmm_map_page_nolock((uint64_t)pdpt_phys, TMP_VA, VMM_FLAG_PRESENT | VMM_FLAG_RW);
    uint64_t* pdpt_tmp = (uint64_t*)(uintptr_t)TMP_VA;
    memset(pdpt_tmp, 0, PAGE_SIZE);
    for (int i = 0; i < 4; i++) {
        pdpt_tmp[i] = (uint64_t)pd_phys[i] | 0x1ULL; /* PRESENT */
    }
    vmm_unmap_page_nolock(TMP_VA);

    /* --- Initialize each PD --- */
    for (int i = 0; i < 4; i++) {
        vmm_map_page_nolock((uint64_t)pd_phys[i], TMP_VA, VMM_FLAG_PRESENT | VMM_FLAG_RW);
        uint64_t* pd_tmp = (uint64_t*)(uintptr_t)TMP_VA;
        memset(pd_tmp, 0, PAGE_SIZE);

        if (i == 3) {
            /* Copy kernel PDEs from current PD[3] */
            volatile uint64_t* active_pd3 = pae_pd_recursive(3);
            for (int j = 0; j < 512; j++) {
                pd_tmp[j] = (uint64_t)active_pd3[j];
            }
            /* Fix recursive mapping: PD[3][508..511] -> new PD[0..3] */
            pd_tmp[508] = (uint64_t)pd_phys[0] | X86_PTE_PRESENT | X86_PTE_RW;
            pd_tmp[509] = (uint64_t)pd_phys[1] | X86_PTE_PRESENT | X86_PTE_RW;
            pd_tmp[510] = (uint64_t)pd_phys[2] | X86_PTE_PRESENT | X86_PTE_RW;
            pd_tmp[511] = (uint64_t)pd_phys[3] | X86_PTE_PRESENT | X86_PTE_RW;
        }

        vmm_unmap_page_nolock(TMP_VA);
    }

    spin_unlock_irqrestore(&vmm_lock, irqf);
    return (uintptr_t)pdpt_phys;
}

void vmm_as_destroy(uintptr_t as) {
    if (!as) return;
    if (as == g_kernel_as) return;

    uintptr_t irqf = spin_lock_irqsave(&vmm_lock);

    uintptr_t old_as = hal_cpu_get_address_space();
    vmm_as_activate(as);

    /* Free user page tables + frames (PDPT[0..2]) */
    for (uint32_t pi = 0; pi < PAE_USER_PDPT_MAX; pi++) {
        volatile uint64_t* pd = pae_pd_recursive(pi);
        for (uint32_t di = 0; di < 512; di++) {
            uint64_t pde = pd[di];
            if ((pde & X86_PTE_PRESENT) == 0) continue;

            uint32_t pt_phys = (uint32_t)(pde & 0xFFFFF000ULL);
            volatile uint64_t* pt = pae_pt_recursive(pi, di);

            for (int ti = 0; ti < 512; ti++) {
                uint64_t pte = pt[ti];
                if ((pte & X86_PTE_PRESENT) == 0) continue;
                uint32_t frame = (uint32_t)(pte & 0xFFFFF000ULL);
                pmm_free_page((void*)(uintptr_t)frame);
                pt[ti] = 0;
            }

            pmm_free_page((void*)(uintptr_t)pt_phys);
            pd[di] = 0;
        }
    }

    /* Read PD physical addresses from PD[3] recursive entries before switching away */
    volatile uint64_t* pd3 = pae_pd_recursive(3);
    uint32_t pd_phys[4];
    pd_phys[0] = (uint32_t)(pd3[508] & 0xFFFFF000ULL);
    pd_phys[1] = (uint32_t)(pd3[509] & 0xFFFFF000ULL);
    pd_phys[2] = (uint32_t)(pd3[510] & 0xFFFFF000ULL);
    pd_phys[3] = (uint32_t)(pd3[511] & 0xFFFFF000ULL);

    vmm_as_activate(old_as);

    /* Free PDs and PDPT */
    for (int i = 0; i < 4; i++) {
        if (pd_phys[i]) pmm_free_page((void*)(uintptr_t)pd_phys[i]);
    }
    pmm_free_page((void*)(uintptr_t)as);

    spin_unlock_irqrestore(&vmm_lock, irqf);
}

uintptr_t vmm_as_clone_user(uintptr_t src_as) {
    if (!src_as) return 0;

    const uintptr_t TMP_MAP_VA = 0xBFF00000U;

    uintptr_t new_as = vmm_as_create_kernel_clone();
    if (!new_as) return 0;

    uint8_t* tmp = (uint8_t*)kmalloc(4096);
    if (!tmp) {
        vmm_as_destroy(new_as);
        return 0;
    }

    uintptr_t irqf = spin_lock_irqsave(&vmm_lock);

    uintptr_t old_as = hal_cpu_get_address_space();
    vmm_as_activate(src_as);

    for (uint32_t pi = 0; pi < PAE_USER_PDPT_MAX; pi++) {
        volatile uint64_t* src_pd = pae_pd_recursive(pi);
        for (uint32_t di = 0; di < 512; di++) {
            uint64_t pde = src_pd[di];
            if ((pde & X86_PTE_PRESENT) == 0) continue;

            volatile uint64_t* src_pt = pae_pt_recursive(pi, di);
            for (uint32_t ti = 0; ti < 512; ti++) {
                uint64_t pte = src_pt[ti];
                if (!(pte & X86_PTE_PRESENT)) continue;
                if (!(pte & X86_PTE_USER)) continue;

                uint32_t flags = VMM_FLAG_PRESENT;
                if (pte & X86_PTE_RW)   flags |= VMM_FLAG_RW;
                if (pte & X86_PTE_USER) flags |= VMM_FLAG_USER;

                void* dst_frame = pmm_alloc_page();
                if (!dst_frame) {
                    vmm_as_activate(old_as);
                    spin_unlock_irqrestore(&vmm_lock, irqf);
                    kfree(tmp);
                    vmm_as_destroy(new_as);
                    return 0;
                }

                uint32_t src_frame = (uint32_t)(pte & 0xFFFFF000ULL);
                uintptr_t va = ((uintptr_t)pi << 30) | ((uintptr_t)di << 21) | ((uintptr_t)ti << 12);

                vmm_as_map_page_nolock(new_as, (uint64_t)(uintptr_t)dst_frame, (uint64_t)va, flags);

                vmm_map_page_nolock((uint64_t)src_frame, (uint64_t)TMP_MAP_VA, VMM_FLAG_PRESENT | VMM_FLAG_RW);
                memcpy(tmp, (const void*)TMP_MAP_VA, 4096);
                vmm_unmap_page_nolock((uint64_t)TMP_MAP_VA);

                vmm_as_activate(new_as);
                vmm_map_page_nolock((uint64_t)(uintptr_t)dst_frame, (uint64_t)TMP_MAP_VA, VMM_FLAG_PRESENT | VMM_FLAG_RW);
                memcpy((void*)TMP_MAP_VA, tmp, 4096);
                vmm_unmap_page_nolock((uint64_t)TMP_MAP_VA);

                vmm_as_activate(src_as);
            }
        }
    }

    vmm_as_activate(old_as);

    spin_unlock_irqrestore(&vmm_lock, irqf);
    kfree(tmp);
    return new_as;
}

uintptr_t vmm_as_clone_user_cow(uintptr_t src_as) {
    if (!src_as) return 0;

    uintptr_t new_as = vmm_as_create_kernel_clone();
    if (!new_as) return 0;

    uintptr_t irqf = spin_lock_irqsave(&vmm_lock);

    uintptr_t old_as = hal_cpu_get_address_space();
    vmm_as_activate(src_as);

    for (uint32_t pi = 0; pi < PAE_USER_PDPT_MAX; pi++) {
        volatile uint64_t* src_pd = pae_pd_recursive(pi);
        for (uint32_t di = 0; di < 512; di++) {
            uint64_t pde = src_pd[di];
            if ((pde & X86_PTE_PRESENT) == 0) continue;

            volatile uint64_t* src_pt = pae_pt_recursive(pi, di);
            for (uint32_t ti = 0; ti < 512; ti++) {
                uint64_t pte = src_pt[ti];
                if (!(pte & X86_PTE_PRESENT)) continue;
                if (!(pte & X86_PTE_USER)) continue;

                uint32_t frame_phys = (uint32_t)(pte & 0xFFFFF000ULL);
                uintptr_t va = ((uintptr_t)pi << 30) | ((uintptr_t)di << 21) | ((uintptr_t)ti << 12);

                uint64_t new_pte = (uint64_t)frame_phys | X86_PTE_PRESENT | X86_PTE_USER;
                if (pte & X86_PTE_RW) {
                    new_pte |= X86_PTE_COW;
                    src_pt[ti] = new_pte;
                    invlpg(va);
                } else {
                    new_pte = pte;
                }

                pmm_incref((uintptr_t)frame_phys);

                vmm_as_map_page_nolock(new_as, (uint64_t)frame_phys, (uint64_t)va,
                                       VMM_FLAG_PRESENT | VMM_FLAG_USER |
                                       ((new_pte & X86_PTE_COW) ? VMM_FLAG_COW : 0));
            }
        }
    }

    vmm_as_activate(old_as);

    spin_unlock_irqrestore(&vmm_lock, irqf);
    return new_as;
}

int vmm_handle_cow_fault(uintptr_t fault_addr) {
    uintptr_t irqf = spin_lock_irqsave(&vmm_lock);

    uintptr_t va = fault_addr & ~(uintptr_t)0xFFF;
    uint32_t pi = pae_pdpt_index((uint64_t)va);
    uint32_t di = pae_pd_index((uint64_t)va);
    uint32_t ti = pae_pt_index((uint64_t)va);

    if (pi >= PAE_USER_PDPT_MAX) {
        spin_unlock_irqrestore(&vmm_lock, irqf);
        return 0;
    }

    volatile uint64_t* pd = pae_pd_recursive(pi);
    if ((pd[di] & X86_PTE_PRESENT) == 0) {
        spin_unlock_irqrestore(&vmm_lock, irqf);
        return 0;
    }

    volatile uint64_t* pt = pae_pt_recursive(pi, di);
    uint64_t pte = pt[ti];

    if (!(pte & X86_PTE_PRESENT)) {
        spin_unlock_irqrestore(&vmm_lock, irqf);
        return 0;
    }
    if (!(pte & X86_PTE_COW)) {
        spin_unlock_irqrestore(&vmm_lock, irqf);
        return 0;
    }

    uint32_t old_frame = (uint32_t)(pte & 0xFFFFF000ULL);
    uint16_t rc = pmm_get_refcount((uintptr_t)old_frame);

    if (rc <= 1) {
        pt[ti] = (uint64_t)old_frame | X86_PTE_PRESENT | X86_PTE_RW | X86_PTE_USER;
        invlpg(va);
        spin_unlock_irqrestore(&vmm_lock, irqf);
        return 1;
    }

    void* new_frame = pmm_alloc_page();
    if (!new_frame) {
        spin_unlock_irqrestore(&vmm_lock, irqf);
        return 0;
    }

    const uintptr_t TMP_COW_VA = 0xBFFFD000U;
    vmm_map_page_nolock((uint64_t)(uintptr_t)new_frame, (uint64_t)TMP_COW_VA,
                        VMM_FLAG_PRESENT | VMM_FLAG_RW);
    memcpy((void*)TMP_COW_VA, (const void*)va, 4096);
    vmm_unmap_page_nolock((uint64_t)TMP_COW_VA);

    pmm_decref((uintptr_t)old_frame);

    pt[ti] = (uint64_t)(uintptr_t)new_frame | X86_PTE_PRESENT | X86_PTE_RW | X86_PTE_USER;
    invlpg(va);

    spin_unlock_irqrestore(&vmm_lock, irqf);
    return 1;
}

uintptr_t vmm_find_free_area(uintptr_t start, uintptr_t end, uint32_t length) {
    if (length == 0) return 0;
    uint32_t pages_needed = (length + 0xFFFU) >> 12;

    uintptr_t irqf = spin_lock_irqsave(&vmm_lock);

    uintptr_t run_start = start & ~(uintptr_t)0xFFF;
    uint32_t run_len = 0;

    for (uintptr_t va = run_start; va < end; va += 0x1000U) {
        uint32_t pi = pae_pdpt_index((uint64_t)va);
        uint32_t di = pae_pd_index((uint64_t)va);
        uint32_t ti = pae_pt_index((uint64_t)va);

        int mapped = 0;
        volatile uint64_t* pd = pae_pd_recursive(pi);
        if (pd[di] & X86_PTE_PRESENT) {
            volatile uint64_t* pt = pae_pt_recursive(pi, di);
            if (pt[ti] & X86_PTE_PRESENT)
                mapped = 1;
        }

        if (!mapped) {
            if (run_len == 0) run_start = va;
            run_len++;
            if (run_len >= pages_needed) {
                spin_unlock_irqrestore(&vmm_lock, irqf);
                return run_start;
            }
        } else {
            run_len = 0;
        }
    }

    spin_unlock_irqrestore(&vmm_lock, irqf);
    return 0;
}

void vmm_init(void) {
    kprintf("[VMM] PAE paging active.\n");

    g_kernel_as = hal_cpu_get_address_space();

    /* Test mapping */
    vmm_map_page(0xB8000, 0xC00B8000, VMM_FLAG_PRESENT | VMM_FLAG_RW);
    kprintf("[VMM] Mapped VGA to 0xC00B8000.\n");
}
