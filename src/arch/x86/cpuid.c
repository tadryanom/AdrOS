#include "arch/x86/cpuid.h"
#include "uart_console.h"
#include "utils.h"

#include <stddef.h>
#include <stdint.h>

static inline void cpuid(uint32_t leaf, uint32_t* eax, uint32_t* ebx,
                          uint32_t* ecx, uint32_t* edx) {
    __asm__ volatile("cpuid"
        : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
        : "a"(leaf), "c"(0));
}

void x86_cpuid_detect(struct x86_cpu_features* out) {
    uint32_t eax, ebx, ecx, edx;

    /* Zero everything */
    for (size_t i = 0; i < sizeof(*out); i++)
        ((uint8_t*)out)[i] = 0;

    /* Leaf 0: vendor string + max standard leaf */
    cpuid(0, &eax, &ebx, &ecx, &edx);
    out->max_leaf = eax;

    /* Vendor: EBX-EDX-ECX (use memcpy to avoid strict-aliasing UB) */
    memcpy(&out->vendor[0], &ebx, 4);
    memcpy(&out->vendor[4], &edx, 4);
    memcpy(&out->vendor[8], &ecx, 4);
    out->vendor[12] = '\0';

    if (out->max_leaf < 1) return;

    /* Leaf 1: feature flags */
    cpuid(1, &eax, &ebx, &ecx, &edx);

    /* ECX features */
    out->sse3       = (ecx >> 0)  & 1;
    out->ssse3      = (ecx >> 9)  & 1;
    out->sse41      = (ecx >> 19) & 1;
    out->sse42      = (ecx >> 20) & 1;
    out->x2apic     = (ecx >> 21) & 1;
    out->avx        = (ecx >> 28) & 1;
    out->hypervisor = (ecx >> 31) & 1;

    /* EDX features */
    out->fpu   = (edx >> 0)  & 1;
    out->tsc   = (edx >> 4)  & 1;
    out->msr   = (edx >> 5)  & 1;
    out->pae   = (edx >> 6)  & 1;
    out->cx8   = (edx >> 8)  & 1;
    out->apic  = (edx >> 9)  & 1;
    out->sep   = (edx >> 11) & 1;
    out->mtrr  = (edx >> 12) & 1;
    out->pge   = (edx >> 13) & 1;
    out->cmov  = (edx >> 15) & 1;
    out->pat   = (edx >> 16) & 1;
    out->pse36 = (edx >> 17) & 1;
    out->mmx   = (edx >> 23) & 1;
    out->fxsr  = (edx >> 24) & 1;
    out->sse   = (edx >> 25) & 1;
    out->sse2  = (edx >> 26) & 1;
    out->htt   = (edx >> 28) & 1;

    /* Topology from EBX */
    out->initial_apic_id = (uint8_t)(ebx >> 24);
    out->logical_cpus    = (uint8_t)(ebx >> 16);

    /* Extended leaves */
    cpuid(0x80000000, &eax, &ebx, &ecx, &edx);
    out->max_ext_leaf = eax;

    if (out->max_ext_leaf >= 0x80000001) {
        cpuid(0x80000001, &eax, &ebx, &ecx, &edx);
        out->nx      = (edx >> 20) & 1;
        out->lm      = (edx >> 29) & 1;
        out->syscall = (edx >> 11) & 1;
    }

    /* Brand string (leaves 0x80000002 - 0x80000004) */
    if (out->max_ext_leaf >= 0x80000004) {
        uint32_t* b = (uint32_t*)out->brand;
        for (uint32_t leaf = 0x80000002; leaf <= 0x80000004; leaf++) {
            cpuid(leaf, &eax, &ebx, &ecx, &edx);
            *b++ = eax;
            *b++ = ebx;
            *b++ = ecx;
            *b++ = edx;
        }
        out->brand[48] = '\0';
    }
}

void x86_cpuid_print(const struct x86_cpu_features* f) {
    uart_print("[CPUID] Vendor: ");
    uart_print(f->vendor);
    uart_print("\n");

    if (f->brand[0]) {
        uart_print("[CPUID] Brand:  ");
        uart_print(f->brand);
        uart_print("\n");
    }

    uart_print("[CPUID] Features:");
    if (f->fpu)   uart_print(" FPU");
    if (f->tsc)   uart_print(" TSC");
    if (f->msr)   uart_print(" MSR");
    if (f->pae)   uart_print(" PAE");
    if (f->apic)  uart_print(" APIC");
    if (f->sep)   uart_print(" SEP");
    if (f->pge)   uart_print(" PGE");
    if (f->mmx)   uart_print(" MMX");
    if (f->fxsr)  uart_print(" FXSR");
    if (f->sse)   uart_print(" SSE");
    if (f->sse2)  uart_print(" SSE2");
    if (f->sse3)  uart_print(" SSE3");
    if (f->ssse3) uart_print(" SSSE3");
    if (f->sse41) uart_print(" SSE4.1");
    if (f->sse42) uart_print(" SSE4.2");
    if (f->avx)   uart_print(" AVX");
    if (f->htt)   uart_print(" HTT");
    if (f->nx)    uart_print(" NX");
    if (f->lm)    uart_print(" LM");
    if (f->x2apic) uart_print(" x2APIC");
    if (f->hypervisor) uart_print(" HYPERVISOR");
    if (f->syscall) uart_print(" SYSCALL");
    uart_print("\n");

    uart_print("[CPUID] APIC ID: ");
    char tmp[12];
    itoa(f->initial_apic_id, tmp, 10);
    uart_print(tmp);
    uart_print(", Logical CPUs: ");
    itoa(f->logical_cpus, tmp, 10);
    uart_print(tmp);
    uart_print("\n");
}
