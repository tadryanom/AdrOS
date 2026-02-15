#ifndef ARCH_X86_SPINLOCK_H
#define ARCH_X86_SPINLOCK_H

#include <stdint.h>

static inline void cpu_relax(void) {
    __asm__ volatile("pause" ::: "memory");
}

static inline uintptr_t irq_save(void) {
    uintptr_t flags;
#if defined(__x86_64__)
    __asm__ volatile ("pushfq; pop %0; cli" : "=r"(flags) :: "memory");
#else
    __asm__ volatile ("pushf; pop %0; cli" : "=r"(flags) :: "memory");
#endif
    return flags;
}

static inline void irq_restore(uintptr_t flags) {
#if defined(__x86_64__)
    __asm__ volatile ("push %0; popfq" :: "r"(flags) : "memory", "cc");
#else
    __asm__ volatile ("push %0; popf" :: "r"(flags) : "memory", "cc");
#endif
}

#endif /* ARCH_X86_SPINLOCK_H */
