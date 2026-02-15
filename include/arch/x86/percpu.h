#ifndef ARCH_X86_PERCPU_H
#define ARCH_X86_PERCPU_H

#include <stdint.h>

/* Forward declarations */
struct process;
struct runqueue;

/* Per-CPU data block — one per CPU, accessed via GS segment.
 * The GS base for each CPU points to its own percpu_data instance. */
struct percpu_data {
    uint32_t         cpu_index;       /* 0 = BSP */
    uint32_t         lapic_id;
    struct process*  current_process; /* Currently running process on this CPU */
    uintptr_t        kernel_stack;    /* Top of this CPU's kernel stack */
    uint32_t         nested_irq;      /* IRQ nesting depth */
    uint32_t         rq_load;         /* Number of READY processes on this CPU */
    uint32_t         reserved[2];     /* Padding to 32 bytes */
};

/* Initialize per-CPU data for all CPUs. Called once from BSP after SMP init. */
void percpu_init(void);

/* Set up GS segment for the current CPU (called by each CPU during init). */
void percpu_setup_gs(uint32_t cpu_index);

/* Get pointer to current CPU's percpu_data (via GS segment). */
static inline struct percpu_data* percpu_get(void) {
    struct percpu_data* p;
    __asm__ volatile("mov %%gs:0, %0" : "=r"(p));
    return p;
}

/* Get current CPU index (fast path via GS). */
static inline uint32_t percpu_cpu_index(void) {
    uint32_t idx;
    __asm__ volatile("mov %%gs:0, %0" : "=r"(idx));
    return idx;
}

/* Get current process on this CPU (fast path via GS). */
static inline struct process* percpu_current(void) {
    struct process* p;
    __asm__ volatile("mov %%gs:8, %0" : "=r"(p));
    return p;
}

/* Set current process on this CPU. */
static inline void percpu_set_current(struct process* proc) {
    __asm__ volatile("mov %0, %%gs:8" : : "r"(proc) : "memory");
}

#endif
