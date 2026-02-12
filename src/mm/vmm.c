#include "vmm.h"
#include "hal/cpu.h"

/*
 * Architecture-independent VMM wrappers.
 *
 * These functions implement generic logic on top of the arch-specific
 * primitives (vmm_map_page, vmm_set_page_flags, etc.) which are
 * provided by src/arch/<ARCH>/vmm.c.
 */

void vmm_protect_range(uint64_t vaddr, uint64_t len, uint32_t flags) {
    if (len == 0) return;
    uint64_t start = vaddr & ~0xFFFULL;
    uint64_t end = (vaddr + len - 1) & ~0xFFFULL;
    for (uint64_t va = start;; va += 0x1000ULL) {
        vmm_set_page_flags(va, flags | VMM_FLAG_PRESENT);
        if (va == end) break;
    }
}

void vmm_as_activate(uintptr_t as) {
    if (!as) return;
    hal_cpu_set_address_space(as);
}

void vmm_as_map_page(uintptr_t as, uint64_t phys, uint64_t virt, uint32_t flags) {
    if (!as) return;
    uintptr_t old_as = hal_cpu_get_address_space();
    if (old_as != as) {
        vmm_as_activate(as);
        vmm_map_page(phys, virt, flags);
        vmm_as_activate(old_as);
    } else {
        vmm_map_page(phys, virt, flags);
    }
}
