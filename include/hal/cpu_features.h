#ifndef HAL_CPU_FEATURES_H
#define HAL_CPU_FEATURES_H

#include <stdint.h>

/* Architecture-independent CPU feature flags */
struct cpu_features {
    char vendor[13];
    char brand[49];

    uint8_t has_apic       : 1;
    uint8_t has_x2apic     : 1;
    uint8_t has_pae        : 1;
    uint8_t has_nx         : 1;
    uint8_t has_sse        : 1;
    uint8_t has_sse2       : 1;
    uint8_t has_fxsr       : 1;
    uint8_t has_sysenter   : 1;  /* x86 SEP / ARM SVC / RISC-V ECALL */
    uint8_t has_syscall    : 1;  /* x86-64 SYSCALL/SYSRET */
    uint8_t has_htt        : 1;  /* Hyper-Threading / SMT */
    uint8_t has_tsc        : 1;
    uint8_t has_msr        : 1;
    uint8_t is_hypervisor  : 1;

    uint8_t logical_cpus;        /* max logical CPUs per package */
    uint8_t initial_cpu_id;      /* BSP APIC ID or equivalent */
};

/* Detect and cache CPU features. Call once during early boot. */
void hal_cpu_detect_features(void);

/* Get pointer to the cached feature struct (valid after hal_cpu_detect_features). */
const struct cpu_features* hal_cpu_get_features(void);

/* Print detected features (UART). */
void hal_cpu_print_features(void);

#endif
