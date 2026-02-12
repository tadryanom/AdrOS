#ifndef HAL_CPU_H
#define HAL_CPU_H

#include <stdint.h>

uintptr_t hal_cpu_get_stack_pointer(void);
uintptr_t hal_cpu_get_address_space(void);
void hal_cpu_set_address_space(uintptr_t as);

void hal_cpu_set_kernel_stack(uintptr_t sp_top);

void hal_cpu_enable_interrupts(void);
void hal_cpu_disable_interrupts(void);
void hal_cpu_idle(void);

uint64_t hal_cpu_read_timestamp(void);

#endif
