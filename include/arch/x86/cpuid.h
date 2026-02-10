#ifndef ARCH_X86_CPUID_H
#define ARCH_X86_CPUID_H

#include <stdint.h>

struct x86_cpu_features {
    /* CPUID leaf 0 */
    uint32_t max_leaf;
    char vendor[13];          /* "GenuineIntel" / "AuthenticAMD" */

    /* CPUID leaf 1 — ECX */
    uint8_t sse3      : 1;
    uint8_t ssse3     : 1;
    uint8_t sse41     : 1;
    uint8_t sse42     : 1;
    uint8_t x2apic    : 1;
    uint8_t avx       : 1;
    uint8_t hypervisor : 1;

    /* CPUID leaf 1 — EDX */
    uint8_t fpu       : 1;
    uint8_t tsc       : 1;
    uint8_t msr       : 1;
    uint8_t pae       : 1;
    uint8_t cx8       : 1;    /* CMPXCHG8B */
    uint8_t apic      : 1;
    uint8_t sep       : 1;    /* SYSENTER/SYSEXIT */
    uint8_t mtrr      : 1;
    uint8_t pge       : 1;    /* Page Global Enable */
    uint8_t cmov      : 1;
    uint8_t pat       : 1;
    uint8_t pse36     : 1;
    uint8_t mmx       : 1;
    uint8_t fxsr      : 1;    /* FXSAVE/FXRSTOR */
    uint8_t sse       : 1;
    uint8_t sse2      : 1;
    uint8_t htt       : 1;    /* Hyper-Threading */

    /* CPUID leaf 0x80000001 — EDX */
    uint8_t nx        : 1;    /* No-Execute (NX / XD) */
    uint8_t lm        : 1;    /* Long Mode (64-bit) */
    uint8_t syscall   : 1;    /* SYSCALL/SYSRET */

    /* Extended info */
    uint32_t max_ext_leaf;
    char brand[49];           /* CPU brand string (leaves 0x80000002-4) */

    /* Topology (from leaf 1 EBX) */
    uint8_t initial_apic_id;
    uint8_t logical_cpus;     /* max logical CPUs per package */
};

/* Detect CPU features. Call once during early boot. */
void x86_cpuid_detect(struct x86_cpu_features* out);

/* Print detected features to UART. */
void x86_cpuid_print(const struct x86_cpu_features* f);

#endif
