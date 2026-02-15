#ifndef ARCH_MIPS_SPINLOCK_H
#define ARCH_MIPS_SPINLOCK_H

#include <stdint.h>

static inline void cpu_relax(void) {
    __asm__ volatile("pause" ::: "memory");
}

static inline uintptr_t irq_save(void) {
    uintptr_t status;
    __asm__ volatile(
        "mfc0 %0, $12\n\t"
        "di"
        : "=r"(status) :: "memory");
    return status & 1U;
}

static inline void irq_restore(uintptr_t flags) {
    if (flags) {
        __asm__ volatile("ei" ::: "memory");
    }
}

#endif /* ARCH_MIPS_SPINLOCK_H */
