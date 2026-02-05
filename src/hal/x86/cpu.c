#include "hal/cpu.h"

#include "gdt.h"

#if defined(__i386__) || defined(__x86_64__)

uintptr_t hal_cpu_get_stack_pointer(void) {
    uintptr_t sp;
#if defined(__x86_64__)
    __asm__ volatile("mov %%rsp, %0" : "=r"(sp));
#else
    __asm__ volatile("mov %%esp, %0" : "=r"(sp));
#endif
    return sp;
}

uintptr_t hal_cpu_get_address_space(void) {
    uintptr_t as;
#if defined(__x86_64__)
    __asm__ volatile("mov %%cr3, %0" : "=r"(as));
#else
    __asm__ volatile("mov %%cr3, %0" : "=r"(as));
#endif
    return as;
}

void hal_cpu_set_kernel_stack(uintptr_t sp_top) {
    tss_set_kernel_stack(sp_top);
}

void hal_cpu_enable_interrupts(void) {
    __asm__ volatile("sti");
}

void hal_cpu_idle(void) {
    __asm__ volatile("hlt");
}

#else

uintptr_t hal_cpu_get_stack_pointer(void) {
    return 0;
}

uintptr_t hal_cpu_get_address_space(void) {
    return 0;
}

void hal_cpu_set_kernel_stack(uintptr_t sp_top) {
    (void)sp_top;
}

void hal_cpu_enable_interrupts(void) {
}

void hal_cpu_idle(void) {
}

#endif
