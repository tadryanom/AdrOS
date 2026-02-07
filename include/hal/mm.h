#ifndef HAL_MM_H
#define HAL_MM_H

#include <stdint.h>

#define HAL_MM_MAP_RW  (1u << 0)

int hal_mm_map_physical_range(uintptr_t phys_start, uintptr_t phys_end, uint32_t flags, uintptr_t* out_virt);

#endif
