#include "hal/mm.h"

int hal_mm_map_physical_range(uintptr_t phys_start, uintptr_t phys_end, uint32_t flags, uintptr_t* out_virt) {
    (void)phys_start;
    (void)phys_end;
    (void)flags;
    (void)out_virt;
    return -1;
}

uintptr_t hal_mm_phys_to_virt(uintptr_t phys) {
    return phys;
}

uintptr_t hal_mm_virt_to_phys(uintptr_t virt) {
    return virt;
}

uintptr_t hal_mm_kernel_virt_base(void) {
    return 0;
}
