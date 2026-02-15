#ifndef SPINLOCK_H
#define SPINLOCK_H

#include <stdint.h>
#include <stddef.h>

/*
 * Architecture-specific primitives: cpu_relax(), irq_save(), irq_restore().
 * Each arch header provides these as static inline functions with the
 * appropriate inline assembly.  The agnostic layer below only uses them
 * through the function interface — no arch #ifdefs leak here.
 */
#if defined(__i386__) || defined(__x86_64__)
#include "arch/x86/spinlock.h"
#elif defined(__aarch64__) || defined(__arm__)
#include "arch/arm/spinlock.h"
#elif defined(__riscv)
#include "arch/riscv/spinlock.h"
#elif defined(__mips__)
#include "arch/mips/spinlock.h"
#else
/* Generic fallback — no interrupt masking, memory barrier as relax hint */
static inline void cpu_relax(void) { __sync_synchronize(); }
static inline uintptr_t irq_save(void) { return 0; }
static inline void irq_restore(uintptr_t flags) { (void)flags; }
#endif

/* ------------------------------------------------------------------ */
/*  Architecture-agnostic spinlock implementation                     */
/* ------------------------------------------------------------------ */

#ifdef SPINLOCK_DEBUG

typedef struct {
    volatile uint32_t locked;
    const char*       name;
    volatile int      holder_cpu;
    volatile uint32_t nest_count;
} spinlock_t;

#define SPINLOCK_INIT(n) { .locked = 0, .name = (n), .holder_cpu = -1, .nest_count = 0 }

static inline void spinlock_init(spinlock_t* l) {
    l->locked = 0;
    l->name = "<unnamed>";
    l->holder_cpu = -1;
    l->nest_count = 0;
}

static inline void spinlock_init_named(spinlock_t* l, const char* name) {
    l->locked = 0;
    l->name = name;
    l->holder_cpu = -1;
    l->nest_count = 0;
}

#else /* !SPINLOCK_DEBUG */

typedef struct {
    volatile uint32_t locked;
} spinlock_t;

#define SPINLOCK_INIT(n) { .locked = 0 }

static inline void spinlock_init(spinlock_t* l) {
    l->locked = 0;
}

static inline void spinlock_init_named(spinlock_t* l, const char* name) {
    (void)name;
    l->locked = 0;
}

#endif /* SPINLOCK_DEBUG */

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
#ifdef SPINLOCK_DEBUG
    l->holder_cpu = 0;
    l->nest_count = 1;
#endif
}

static inline int spin_trylock(spinlock_t* l) {
    if (l->locked) return 0;
    l->locked = 1;
    __sync_synchronize();
#ifdef SPINLOCK_DEBUG
    l->holder_cpu = 0;
    l->nest_count = 1;
#endif
    return 1;
}

static inline void spin_unlock(spinlock_t* l) {
#ifdef SPINLOCK_DEBUG
    l->holder_cpu = -1;
    l->nest_count = 0;
#endif
    __sync_synchronize();
    l->locked = 0;
}
#else
static inline void spin_lock(spinlock_t* l) {
#ifdef SPINLOCK_DEBUG
    uint32_t spins = 0;
#endif
    while (__sync_lock_test_and_set(&l->locked, 1)) {
        while (l->locked) {
            cpu_relax();
#ifdef SPINLOCK_DEBUG
            if (++spins > 10000000) {
                extern void kprintf(const char* fmt, ...);
                kprintf("[SPINLOCK] deadlock? lock '%s' held by cpu %d\n",
                        l->name ? l->name : "?", l->holder_cpu);
                spins = 0;
            }
#endif
        }
    }
#ifdef SPINLOCK_DEBUG
    {
        extern uint32_t lapic_get_id(void);
        l->holder_cpu = (int)lapic_get_id();
    }
    l->nest_count = 1;
#endif
}

static inline int spin_trylock(spinlock_t* l) {
    if (__sync_lock_test_and_set(&l->locked, 1) != 0) return 0;
#ifdef SPINLOCK_DEBUG
    {
        extern uint32_t lapic_get_id(void);
        l->holder_cpu = (int)lapic_get_id();
    }
    l->nest_count = 1;
#endif
    return 1;
}

static inline void spin_unlock(spinlock_t* l) {
#ifdef SPINLOCK_DEBUG
    l->holder_cpu = -1;
    l->nest_count = 0;
#endif
    __sync_synchronize();
    __sync_lock_release(&l->locked);
}
#endif

/* ------------------------------------------------------------------ */
/*  Convenience wrappers (fully agnostic)                             */
/* ------------------------------------------------------------------ */

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
