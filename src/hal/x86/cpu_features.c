#include "hal/cpu_features.h"
#include "arch/x86/cpuid.h"
#include "console.h"

#include <stddef.h>

#define CR4_SMEP (1U << 20)
#define CR4_SMAP (1U << 21)

static inline uint32_t read_cr4(void) {
    uint32_t val;
    __asm__ volatile("mov %%cr4, %0" : "=r"(val));
    return val;
}

static inline void write_cr4(uint32_t val) {
    __asm__ volatile("mov %0, %%cr4" :: "r"(val) : "memory");
}

int g_smap_enabled = 0;

static struct cpu_features g_features;
static struct x86_cpu_features g_x86_features;

void hal_cpu_detect_features(void) {
    x86_cpuid_detect(&g_x86_features);

    /* Copy to generic struct */
    for (size_t i = 0; i < 12; i++)
        g_features.vendor[i] = g_x86_features.vendor[i];
    g_features.vendor[12] = '\0';

    for (size_t i = 0; i < 48; i++)
        g_features.brand[i] = g_x86_features.brand[i];
    g_features.brand[48] = '\0';

    g_features.has_apic      = g_x86_features.apic;
    g_features.has_x2apic    = g_x86_features.x2apic;
    g_features.has_pae       = g_x86_features.pae;
    g_features.has_nx        = g_x86_features.nx;
    g_features.has_sse       = g_x86_features.sse;
    g_features.has_sse2      = g_x86_features.sse2;
    g_features.has_fxsr      = g_x86_features.fxsr;
    g_features.has_sysenter  = g_x86_features.sep;
    g_features.has_syscall   = g_x86_features.syscall;
    g_features.has_htt       = g_x86_features.htt;
    g_features.has_tsc       = g_x86_features.tsc;
    g_features.has_msr       = g_x86_features.msr;
    g_features.is_hypervisor = g_x86_features.hypervisor;

    g_features.logical_cpus  = g_x86_features.logical_cpus;
    g_features.initial_cpu_id = g_x86_features.initial_apic_id;

    /* Enable SMEP if supported: prevents kernel from executing user-mapped pages.
     * This blocks a common exploit technique where an attacker maps shellcode in
     * userspace and tricks the kernel into jumping to it. */
    if (g_x86_features.smep) {
        uint32_t cr4 = read_cr4();
        cr4 |= CR4_SMEP;
        write_cr4(cr4);
        kprintf("[CPU] SMEP enabled.\n");
    }

    /* Enable SMAP if supported: prevents kernel from accidentally reading/writing
     * user-mapped pages.  copy_from_user/copy_to_user bracket accesses with
     * STAC/CLAC so legitimate user copies still work. */
    if (g_x86_features.smap) {
        uint32_t cr4 = read_cr4();
        cr4 |= CR4_SMAP;
        write_cr4(cr4);
        g_smap_enabled = 1;
        kprintf("[CPU] SMAP enabled.\n");
    }
}

const struct cpu_features* hal_cpu_get_features(void) {
    return &g_features;
}

void hal_cpu_print_features(void) {
    x86_cpuid_print(&g_x86_features);
}
