#ifndef ARCH_X86_SMP_H
#define ARCH_X86_SMP_H

#include <stdint.h>

/* Maximum number of CPUs supported */
#define SMP_MAX_CPUS 16

/* Per-CPU state */
struct cpu_info {
    uint8_t  lapic_id;       /* LAPIC ID */
    uint8_t  cpu_index;      /* Index in cpu_info array (0 = BSP) */
    uint8_t  started;        /* 1 if AP has completed init */
    uint8_t  reserved;
    uint32_t kernel_stack;   /* Top of this CPU's kernel stack */
};

/* Initialize SMP: discover APs via ACPI, send INIT-SIPI-SIPI.
 * Returns the number of CPUs that started (including BSP). */
int smp_init(void);

/* Get the number of active CPUs. */
uint32_t smp_get_cpu_count(void);

/* Get cpu_info for a given CPU index. */
const struct cpu_info* smp_get_cpu(uint32_t index);

/* Get the current CPU's index (based on LAPIC ID). */
uint32_t smp_current_cpu(void);

#endif
