#ifndef SPINLOCK_H
#define SPINLOCK_H

#include <stdint.h>

#include <stddef.h>

/*
 * Per-architecture spin-wait hint.
 * Reduces power consumption and avoids memory-order pipeline stalls
 * while spinning. Essential for SMP correctness and performance.
 */
static inline void cpu_relax(void) {
#if defined(__i386__) || defined(__x86_64__)
    __asm__ volatile("pause" ::: "memory");
#elif defined(__arm__)
    __asm__ volatile("yield" ::: "memory");
#elif defined(__aarch64__)
    __asm__ volatile("yield" ::: "memory");
#elif defined(__riscv)
    __asm__ volatile("fence" ::: "memory");
#elif defined(__mips__)
    __asm__ volatile("pause" ::: "memory");
#else
    __sync_synchronize();
#endif
}

typedef struct {
    volatile uint32_t locked;
} spinlock_t;

static inline void spinlock_init(spinlock_t* l) {
    l->locked = 0;
}

static inline int spin_is_locked(spinlock_t* l) {
    return l->locked != 0;
}

/*
 * Test-and-test-and-set (TTAS) spinlock.
 * __sync_lock_test_and_set compiles to:
 *   x86:    XCHG (implicit LOCK prefix)
 *   ARM:    LDREX/STREX
 *   RISC-V: AMOSWAP.W.AQ
 *   MIPS:   LL/SC
 *
 * Note: AArch64/RISC-V without MMU may need simpler locking since
 * exclusive monitors (LDAXR/STXR) require cacheable memory.
 */
#if defined(__aarch64__) || defined(__riscv)
/* Simple volatile flag lock — safe for single-core bring-up without MMU.
 * Will be replaced with proper atomics once MMU is enabled. */
static inline void spin_lock(spinlock_t* l) {
    while (l->locked) {
        cpu_relax();
    }
    l->locked = 1;
    __sync_synchronize();
}

static inline int spin_trylock(spinlock_t* l) {
    if (l->locked) return 0;
    l->locked = 1;
    __sync_synchronize();
    return 1;
}

static inline void spin_unlock(spinlock_t* l) {
    __sync_synchronize();
    l->locked = 0;
}
#else
static inline void spin_lock(spinlock_t* l) {
    while (__sync_lock_test_and_set(&l->locked, 1)) {
        while (l->locked) {
            cpu_relax();
        }
    }
}

static inline int spin_trylock(spinlock_t* l) {
    return __sync_lock_test_and_set(&l->locked, 1) == 0;
}

static inline void spin_unlock(spinlock_t* l) {
    __sync_synchronize();
    __sync_lock_release(&l->locked);
}
#endif

#if defined(__i386__) || defined(__x86_64__)
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
#elif defined(__aarch64__)
static inline uintptr_t irq_save(void) {
    uintptr_t daif;
    __asm__ volatile("mrs %0, daif\n\tmsr daifset, #2" : "=r"(daif) :: "memory");
    return daif;
}

static inline void irq_restore(uintptr_t flags) {
    __asm__ volatile("msr daif, %0" :: "r"(flags) : "memory");
}

#elif defined(__arm__)
static inline uintptr_t irq_save(void) {
    uintptr_t cpsr;
    __asm__ volatile("mrs %0, cpsr\n\tcpsid i" : "=r"(cpsr) :: "memory");
    return cpsr;
}

static inline void irq_restore(uintptr_t flags) {
    __asm__ volatile("msr cpsr_c, %0" :: "r"(flags) : "memory");
}

#elif defined(__riscv)
static inline uintptr_t irq_save(void) {
    uintptr_t mstatus;
    __asm__ volatile("csrrci %0, mstatus, 0x8" : "=r"(mstatus) :: "memory");
    return mstatus & 0x8;
}

static inline void irq_restore(uintptr_t flags) {
    if (flags) {
        __asm__ volatile("csrsi mstatus, 0x8" ::: "memory");
    }
}

#elif defined(__mips__)
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

#else
static inline uintptr_t irq_save(void) { return 0; }
static inline void irq_restore(uintptr_t flags) { (void)flags; }
#endif

static inline uintptr_t spin_lock_irqsave(spinlock_t* l) {
    uintptr_t flags = irq_save();
    spin_lock(l);
    return flags;
}

static inline void spin_unlock_irqrestore(spinlock_t* l, uintptr_t flags) {
    spin_unlock(l);
    irq_restore(flags);
}

#endif
