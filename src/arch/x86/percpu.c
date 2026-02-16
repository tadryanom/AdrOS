#include "arch/x86/percpu.h"
#include "arch/x86/smp.h"
#include "arch/x86/gdt.h"
#include "console.h"
#include "utils.h"

#include <stdint.h>
#include <stddef.h>

static struct percpu_data g_percpu[SMP_MAX_CPUS];

/* Lookup table: LAPIC ID → percpu GS selector.
 * Used by the ISR entry stub (interrupts.S) to reload the correct
 * percpu GS without needing GS itself (chicken-and-egg).
 * Indexed by LAPIC ID (0–255), entries are 16-bit GDT selectors. */
uint16_t _percpu_gs_lut[256] __attribute__((aligned(4)));

/* We use GDT entries 6..6+N for per-CPU GS segments.
 * GDT layout: 0=null, 1=kcode, 2=kdata, 3=ucode, 4=udata, 5=TSS, 6+=percpu */
#define PERCPU_GDT_BASE  6

/* Set a GDT entry for a per-CPU GS segment.
 * The segment base points to the percpu_data struct for that CPU.
 * Limit = sizeof(percpu_data) - 1, byte granularity, ring 0 data. */
static void set_percpu_gdt_entry(uint32_t gdt_index, uint32_t base) {
    /* Access byte: Present(1) | DPL(00) | S(1) | Type(0010 = data r/w) = 0x92 */
    /* Granularity: G(0)=byte | D(1)=32bit | L(0) | AVL(0) = 0x40 */
    extern void gdt_set_gate_ext(int num, uint32_t base, uint32_t limit,
                                  uint8_t access, uint8_t gran);
    gdt_set_gate_ext((int)gdt_index, base, sizeof(struct percpu_data) - 1, 0x92, 0x40);
}

void percpu_init(void) {
    uint32_t ncpus = smp_get_cpu_count();
    if (ncpus > SMP_MAX_CPUS) ncpus = SMP_MAX_CPUS;

    /* Clear lookup table — default to BSP's GS selector as fallback */
    uint16_t bsp_sel = (uint16_t)(PERCPU_GDT_BASE * 8);
    for (int j = 0; j < 256; j++) _percpu_gs_lut[j] = bsp_sel;

    for (uint32_t i = 0; i < ncpus; i++) {
        const struct cpu_info* ci = smp_get_cpu(i);
        g_percpu[i].cpu_index = i;
        g_percpu[i].lapic_id = ci ? ci->lapic_id : 0;
        g_percpu[i].current_process = NULL;
        g_percpu[i].kernel_stack = ci ? ci->kernel_stack : 0;
        g_percpu[i].nested_irq = 0;

        /* Create a GDT entry for this CPU's GS segment */
        set_percpu_gdt_entry(PERCPU_GDT_BASE + i, (uint32_t)(uintptr_t)&g_percpu[i]);

        /* Populate LAPIC-ID → GS-selector lookup table */
        uint16_t sel = (uint16_t)((PERCPU_GDT_BASE + i) * 8);
        if (ci) _percpu_gs_lut[ci->lapic_id] = sel;
    }

    kprintf("[PERCPU] Initialized for %u CPU(s).\n", (unsigned)ncpus);
}

void percpu_setup_gs(uint32_t cpu_index) {
    /* GS selector = (PERCPU_GDT_BASE + cpu_index) * 8, RPL=0 */
    uint16_t sel = (uint16_t)((PERCPU_GDT_BASE + cpu_index) * 8);
    __asm__ volatile("mov %0, %%gs" : : "r"(sel));
}

struct percpu_data* percpu_get_ptr(uint32_t cpu_index) {
    if (cpu_index >= SMP_MAX_CPUS) return NULL;
    return &g_percpu[cpu_index];
}
