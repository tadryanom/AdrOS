#include "hal/cpu_features.h"
#include "arch/x86/cpuid.h"

#include <stddef.h>

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
}

const struct cpu_features* hal_cpu_get_features(void) {
    return &g_features;
}

void hal_cpu_print_features(void) {
    x86_cpuid_print(&g_x86_features);
}
