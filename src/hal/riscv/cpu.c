#include "hal/cpu.h"

#if defined(__riscv)

uintptr_t hal_cpu_get_stack_pointer(void) {
    uintptr_t sp;
    __asm__ volatile("mv %0, sp" : "=r"(sp));
    return sp;
}

uintptr_t hal_cpu_get_address_space(void) {
    return 0;
}

void hal_cpu_set_kernel_stack(uintptr_t sp_top) {
    (void)sp_top;
}

void hal_cpu_enable_interrupts(void) {
    __asm__ volatile("csrsi mstatus, 0x8" ::: "memory");
}

void hal_cpu_idle(void) {
    __asm__ volatile("wfi" ::: "memory");
}

#else

uintptr_t hal_cpu_get_stack_pointer(void) { return 0; }
uintptr_t hal_cpu_get_address_space(void) { return 0; }
void hal_cpu_set_kernel_stack(uintptr_t sp_top) { (void)sp_top; }
void hal_cpu_enable_interrupts(void) { }
void hal_cpu_idle(void) { }

#endif
