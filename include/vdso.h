#ifndef VDSO_H
#define VDSO_H

#include <stdint.h>

/* Shared page layout — mapped read-only at VDSO_USER_ADDR in every process */
#define VDSO_USER_ADDR 0x007FE000U  /* one page below stack guard */

struct vdso_data {
    volatile uint32_t tick_count;   /* updated by kernel timer ISR */
    uint32_t tick_hz;               /* ticks per second (e.g. 50) */
};

void vdso_init(void);
void vdso_update_tick(uint32_t tick);

/* Returns the physical address of the vDSO page (for mapping into user AS) */
uintptr_t vdso_get_phys(void);

#endif
