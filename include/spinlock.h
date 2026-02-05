#ifndef SPINLOCK_H
#define SPINLOCK_H

#include <stdint.h>

#include <stddef.h>

typedef struct {
    volatile uint32_t locked;
} spinlock_t;

static inline void spinlock_init(spinlock_t* l) {
    l->locked = 0;
}

static inline void spin_lock(spinlock_t* l) {
    while (__sync_lock_test_and_set(&l->locked, 1)) {
        while (l->locked) {
#if defined(__i386__) || defined(__x86_64__)
            __asm__ volatile ("pause");
#endif
        }
    }
}

static inline void spin_unlock(spinlock_t* l) {
    __sync_lock_release(&l->locked);
}

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
#else
static inline uintptr_t irq_save(void) {
    return 0;
}

static inline void irq_restore(uintptr_t flags) {
    (void)flags;
}
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
