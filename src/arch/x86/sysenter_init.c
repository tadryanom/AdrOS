#include "hal/cpu_features.h"
#include "interrupts.h"
#include "console.h"
#include "arch/x86/smp.h"

#include <stdint.h>

extern void sysenter_entry(void);

/* Write to a Model-Specific Register */
static inline void wrmsr(uint32_t msr, uint64_t value) {
    uint32_t lo = (uint32_t)(value & 0xFFFFFFFF);
    uint32_t hi = (uint32_t)(value >> 32);
    __asm__ volatile("wrmsr" : : "c"(msr), "a"(lo), "d"(hi));
}

static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t lo, hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}

#define IA32_SYSENTER_CS  0x174
#define IA32_SYSENTER_ESP 0x175
#define IA32_SYSENTER_EIP 0x176

/* Per-CPU kernel stacks for SYSENTER entry — used only briefly before
 * the handler switches to the per-task kernel stack via TSS.ESP0.
 * Each CPU needs its own stack to avoid corruption when multiple CPUs
 * enter SYSENTER simultaneously. */
static uint8_t sysenter_stacks[SMP_MAX_CPUS][4096] __attribute__((aligned(16)));
static int sysenter_enabled = 0;

static void x86_sysenter_init(void);

/* Generic syscall_handler defined in src/kernel/syscall.c */
extern void syscall_handler(struct registers*);

void arch_syscall_init(void) {
    register_interrupt_handler(128, syscall_handler);
    x86_sysenter_init();
}

static void x86_sysenter_init(void) {
    const struct cpu_features* f = hal_cpu_get_features();
    if (!f->has_sysenter) {
        kprintf("[SYSENTER] CPU does not support SYSENTER/SYSEXIT.\n");
        return;
    }

    /* MSR 0x174: kernel CS selector. CPU uses CS+8 for kernel SS,
     * CS+16|3 for user CS, CS+24|3 for user SS.
     * Our GDT: 0x08=KernelCS, 0x10=KernelSS, 0x18=UserCS, 0x20=UserSS ✓ */
    wrmsr(IA32_SYSENTER_CS, 0x08);

    /* MSR 0x175: kernel ESP — top of BSP's sysenter stack */
    wrmsr(IA32_SYSENTER_ESP, (uintptr_t)&sysenter_stacks[0][4096]);

    /* MSR 0x176: kernel EIP — our assembly entry point */
    wrmsr(IA32_SYSENTER_EIP, (uintptr_t)sysenter_entry);

    sysenter_enabled = 1;
    kprintf("[SYSENTER] Fast syscall enabled.\n");
}

void x86_sysenter_set_kernel_stack(uintptr_t esp0) {
    if (sysenter_enabled) {
        wrmsr(IA32_SYSENTER_ESP, (uint64_t)esp0);
    }
}

void sysenter_init_ap(uint32_t cpu_index) {
    if (!sysenter_enabled) return;
    if (cpu_index >= SMP_MAX_CPUS) return;
    wrmsr(IA32_SYSENTER_CS, 0x08);
    wrmsr(IA32_SYSENTER_ESP, (uintptr_t)&sysenter_stacks[cpu_index][4096]);
    wrmsr(IA32_SYSENTER_EIP, (uintptr_t)sysenter_entry);
}
