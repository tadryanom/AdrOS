#include "hal/cpu.h"

#if defined(__aarch64__)

uintptr_t hal_cpu_get_stack_pointer(void) {
    uintptr_t sp;
    __asm__ volatile("mov %0, sp" : "=r"(sp));
    return sp;
}

uintptr_t hal_cpu_get_address_space(void) {
    return 0;
}

void hal_cpu_set_kernel_stack(uintptr_t sp_top) {
    (void)sp_top;
}

void hal_cpu_enable_interrupts(void) {
    __asm__ volatile("msr daifclr, #2" ::: "memory");
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
