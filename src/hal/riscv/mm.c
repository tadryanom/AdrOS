#include "hal/mm.h"

int hal_mm_map_physical_range(uintptr_t phys_start, uintptr_t phys_end, uint32_t flags, uintptr_t* out_virt) {
    (void)phys_start;
    (void)phys_end;
    (void)flags;
    (void)out_virt;
    return -1;
}
