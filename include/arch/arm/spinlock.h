#ifndef ARCH_ARM_SPINLOCK_H
#define ARCH_ARM_SPINLOCK_H

#include <stdint.h>

#if defined(__aarch64__)

static inline void cpu_relax(void) {
    __asm__ volatile("yield" ::: "memory");
}

static inline uintptr_t irq_save(void) {
    uintptr_t daif;
    __asm__ volatile("mrs %0, daif\n\tmsr daifset, #2" : "=r"(daif) :: "memory");
    return daif;
}

static inline void irq_restore(uintptr_t flags) {
    __asm__ volatile("msr daif, %0" :: "r"(flags) : "memory");
}

#else /* ARM32 */

static inline void cpu_relax(void) {
    __asm__ volatile("yield" ::: "memory");
}

static inline uintptr_t irq_save(void) {
    uintptr_t cpsr;
    __asm__ volatile("mrs %0, cpsr\n\tcpsid i" : "=r"(cpsr) :: "memory");
    return cpsr;
}

static inline void irq_restore(uintptr_t flags) {
    __asm__ volatile("msr cpsr_c, %0" :: "r"(flags) : "memory");
}

#endif

#endif /* ARCH_ARM_SPINLOCK_H */
