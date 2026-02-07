#include "hal/mm.h"

#include "vmm.h"

#include <stddef.h>

int hal_mm_map_physical_range(uintptr_t phys_start, uintptr_t phys_end, uint32_t flags, uintptr_t* out_virt) {
    if (!out_virt) return -1;

    if (phys_end < phys_start) phys_end = phys_start;

    const uintptr_t virt_base = 0xE0000000U;

    uintptr_t phys_start_aligned = phys_start & ~(uintptr_t)0xFFF;
    uintptr_t phys_end_aligned = (phys_end + 0xFFF) & ~(uintptr_t)0xFFF;

    uintptr_t size = phys_end_aligned - phys_start_aligned;
    uintptr_t pages = size >> 12;

    uint32_t vmm_flags = VMM_FLAG_PRESENT;
    if (flags & HAL_MM_MAP_RW) vmm_flags |= VMM_FLAG_RW;

    uintptr_t virt = virt_base;
    uintptr_t phys = phys_start_aligned;
    for (uintptr_t i = 0; i < pages; i++) {
        vmm_map_page((uint64_t)phys, (uint64_t)virt, vmm_flags);
        phys += 0x1000;
        virt += 0x1000;
    }

    *out_virt = virt_base + (phys_start - phys_start_aligned);
    return 0;
}
