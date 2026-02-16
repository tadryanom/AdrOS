#include "arch/x86/smp.h"
#include "arch/x86/acpi.h"
#include "arch/x86/lapic.h"
#include "arch/x86/percpu.h"
#include "arch/x86/idt.h"
#include "arch/x86/gdt.h"
#include "console.h"
#include "utils.h"
#include "io.h"
#include "hal/cpu.h"

#include <stdint.h>
#include <stddef.h>

/* Trampoline symbols from ap_trampoline.S */
extern uint8_t ap_trampoline_start[];
extern uint8_t ap_trampoline_end[];
extern uint8_t ap_pm_entry[];
extern uint8_t ap_pm_target[];

/* Must match ap_trampoline.S constants */
#define AP_TRAMPOLINE_PHYS  0x8000U
#define AP_DATA_PHYS        0x8F00U

/* Per-AP kernel stack size — must be large enough for sched_ap_init()
 * which calls kmalloc, kstack_alloc, kprintf under sched_lock. */
#define AP_STACK_SIZE       8192U

#define KERNEL_VIRT_BASE    0xC0000000U

static struct cpu_info g_cpus[SMP_MAX_CPUS];
static volatile uint32_t g_cpu_count = 0;

/* Flag set by BSP after process_init + timer_init to signal APs
 * that they can safely initialize their per-CPU schedulers. */
volatile uint32_t ap_sched_go = 0;

/* AP kernel stacks — statically allocated */
static uint8_t ap_stacks[SMP_MAX_CPUS][AP_STACK_SIZE] __attribute__((aligned(16)));

/* Rough busy-wait delay */
static void delay_us(uint32_t us) {
    volatile uint32_t count = us * 10;
    while (count--) {
        __asm__ volatile("pause");
    }
}

/* Read the current CR3 value (page directory physical address) */
static inline uint32_t read_cr3(void) {
    uint32_t val;
    __asm__ volatile("mov %%cr3, %0" : "=r"(val));
    return val;
}

/* Called by each AP after it enters protected mode + paging.
 * This runs on the AP's own stack. */
void ap_entry(void) {
    /* Reload the GDT with virtual base address.  The trampoline loaded
     * the GDT using a physical base for the real→protected mode transition.
     * Now that paging is active we must switch to the virtual base so
     * segment loads, LTR, and ring transitions read the correct GDT. */
    extern struct gdt_ptr gp;
    __asm__ volatile("lgdt %0" : : "m"(gp));

    /* Reload segment registers with the virtual-base GDT */
    __asm__ volatile(
        "mov $0x10, %%ax\n\t"
        "mov %%ax, %%ds\n\t"
        "mov %%ax, %%es\n\t"
        "mov %%ax, %%fs\n\t"
        "mov %%ax, %%ss\n\t"
        "ljmp $0x08, $1f\n\t"
        "1:\n\t"
        ::: "eax", "memory"
    );

    /* Load the IDT on this AP (BSP already initialized it, APs just need lidt) */
    idt_load_ap();

    /* Enable LAPIC on this AP */
    uint64_t apic_base_msr = rdmsr(0x1B);
    if (!(apic_base_msr & (1ULL << 11))) {
        apic_base_msr |= (1ULL << 11);
        wrmsr(0x1B, apic_base_msr);
    }

    /* The LAPIC MMIO is already mapped by the BSP (same page directory).
     * Set spurious vector and enable. */
    lapic_write(LAPIC_SVR, LAPIC_SVR_ENABLE | LAPIC_SPURIOUS_VEC);
    lapic_write(LAPIC_TPR, 0);

    /* Get our LAPIC ID */
    uint32_t my_id = lapic_get_id();

    /* Find our cpu_info slot, set up per-CPU GS, and mark started */
    uint32_t my_cpu = 0;
    for (uint32_t i = 0; i < g_cpu_count; i++) {
        if (g_cpus[i].lapic_id == (uint8_t)my_id) {
            my_cpu = i;
            percpu_setup_gs(i);
            __atomic_store_n(&g_cpus[i].started, 1, __ATOMIC_SEQ_CST);
            break;
        }
    }

    /* Set up per-CPU TSS so this AP can handle ring 0↔3 transitions */
    tss_init_ap(my_cpu);

    /* Set up SYSENTER MSRs on this AP (per-CPU MSRs + per-CPU stack) */
    extern void sysenter_init_ap(uint32_t cpu_index);
    sysenter_init_ap(my_cpu);

    /* Wait for BSP to finish scheduler init (process_init sets PID 0).
     * We check by waiting for the ap_sched_go flag set by the BSP after
     * timer_init completes. */
    while (!__atomic_load_n(&ap_sched_go, __ATOMIC_ACQUIRE)) {
        __asm__ volatile("pause");
    }

    /* Initialize this AP's scheduler: create idle process + set current */
    extern void sched_ap_init(uint32_t);
    sched_ap_init(my_cpu);

    /* Start LAPIC timer using BSP-calibrated ticks */
    lapic_timer_start_ap();
    kprintf("[SMP] CPU%u scheduler active.\n", my_cpu);
    (void)my_cpu;

    /* AP enters idle loop — timer interrupts will call schedule() */
    for (;;) {
        __asm__ volatile("sti; hlt");
    }
}

int smp_enumerate(void) {
    const struct acpi_info* acpi = acpi_get_info();
    if (!acpi || acpi->num_cpus <= 1) {
        g_cpu_count = 1;
        g_cpus[0].lapic_id = (uint8_t)lapic_get_id();
        g_cpus[0].cpu_index = 0;
        g_cpus[0].started = 1;
        g_cpus[0].kernel_stack = 0;
        kprintf("[SMP] Single CPU enumerated.\n");
        return 1;
    }

    g_cpu_count = acpi->num_cpus;
    uint8_t bsp_id = (uint8_t)lapic_get_id();

    for (uint8_t i = 0; i < acpi->num_cpus && i < SMP_MAX_CPUS; i++) {
        g_cpus[i].lapic_id = acpi->cpu_lapic_ids[i];
        g_cpus[i].cpu_index = i;
        g_cpus[i].started = (g_cpus[i].lapic_id == bsp_id) ? 1 : 0;
        g_cpus[i].kernel_stack = (uint32_t)(uintptr_t)&ap_stacks[i][AP_STACK_SIZE];
    }

    kprintf("[SMP] Enumerated %u CPU(s).\n", (unsigned)g_cpu_count);

    return (int)g_cpu_count;
}

int smp_start_aps(void) {
    if (g_cpu_count <= 1) {
        return 1;
    }

    const struct acpi_info* acpi = acpi_get_info();
    if (!acpi) return 1;

    uint8_t bsp_id = (uint8_t)lapic_get_id();

    /* Copy trampoline code to 0x8000 (identity-mapped by boot.S) */
    uint32_t tramp_size = (uint32_t)((uintptr_t)ap_trampoline_end - (uintptr_t)ap_trampoline_start);
    volatile uint8_t* dest = (volatile uint8_t*)(AP_TRAMPOLINE_PHYS + KERNEL_VIRT_BASE);
    for (uint32_t i = 0; i < tramp_size; i++) {
        dest[i] = ap_trampoline_start[i];
    }

    /* Patch the far-jump target: physical address of ap_pm_entry in the copy */
    uint32_t pm_entry_offset = (uint32_t)((uintptr_t)ap_pm_entry - (uintptr_t)ap_trampoline_start);
    uint32_t pm_target_offset = (uint32_t)((uintptr_t)ap_pm_target - (uintptr_t)ap_trampoline_start);
    volatile uint32_t* jmp_target = (volatile uint32_t*)(
        AP_TRAMPOLINE_PHYS + KERNEL_VIRT_BASE + pm_target_offset);
    *jmp_target = AP_TRAMPOLINE_PHYS + pm_entry_offset;

    /* Write data area at 0x8F00 (accessed via identity map + KERNEL_VIRT_BASE) */
    volatile uint8_t* data = (volatile uint8_t*)(AP_DATA_PHYS + KERNEL_VIRT_BASE);

    /* 0x8F00: GDT pointer (6 bytes) with physical base address */
    struct gdt_ptr phys_gp;
    phys_gp.limit = gp.limit;
    phys_gp.base = gp.base - KERNEL_VIRT_BASE;
    memcpy((void*)data, &phys_gp, sizeof(phys_gp));

    /* 0x8F08: CR3 (page directory physical address) */
    uint32_t cr3_val = read_cr3();
    volatile uint32_t* cr3_ptr   = (volatile uint32_t*)(data + 8);
    volatile uint32_t* stack_ptr = (volatile uint32_t*)(data + 12);
    volatile uint32_t* entry_ptr = (volatile uint32_t*)(data + 16);

    *cr3_ptr = cr3_val;
    *entry_ptr = (uint32_t)(uintptr_t)ap_entry;

    kprintf("[SMP] Starting %u AP(s)...\n", (unsigned)(g_cpu_count - 1));

    uint8_t sipi_vector = (uint8_t)(AP_TRAMPOLINE_PHYS >> 12);

    for (uint32_t i = 0; i < g_cpu_count && i < SMP_MAX_CPUS; i++) {
        if (g_cpus[i].lapic_id == bsp_id) continue;
        if (!acpi->cpu_enabled[i]) continue;

        /* Set this AP's stack before sending SIPI */
        *stack_ptr = g_cpus[i].kernel_stack;

        /* INIT IPI */
        lapic_send_ipi(g_cpus[i].lapic_id, 0x00004500);
        delay_us(10000);

        /* INIT deassert */
        lapic_send_ipi(g_cpus[i].lapic_id, 0x00008500);
        delay_us(200);

        /* First SIPI */
        lapic_send_ipi(g_cpus[i].lapic_id, 0x00004600 | sipi_vector);
        delay_us(200);

        /* Second SIPI (per Intel spec) */
        lapic_send_ipi(g_cpus[i].lapic_id, 0x00004600 | sipi_vector);
        delay_us(200);

        /* Wait for AP to signal ready (timeout ~1s) */
        for (uint32_t wait = 0; wait < 100000; wait++) {
            if (__atomic_load_n(&g_cpus[i].started, __ATOMIC_SEQ_CST)) break;
            delay_us(10);
        }

        if (g_cpus[i].started) {
            kprintf("[SMP] CPU %u started.\n", (unsigned)g_cpus[i].lapic_id);
        } else {
            kprintf("[SMP] CPU %u failed to start!\n", (unsigned)g_cpus[i].lapic_id);
        }
    }

    uint32_t started = 0;
    for (uint32_t i = 0; i < g_cpu_count; i++) {
        if (g_cpus[i].started) started++;
    }

    kprintf("[SMP] %u CPU(s) active.\n", (unsigned)started);

    return (int)started;
}

uint32_t smp_get_cpu_count(void) {
    return g_cpu_count;
}

const struct cpu_info* smp_get_cpu(uint32_t index) {
    if (index >= g_cpu_count) return NULL;
    return &g_cpus[index];
}

uint32_t smp_current_cpu(void) {
    uint32_t id = lapic_get_id();
    for (uint32_t i = 0; i < g_cpu_count; i++) {
        if (g_cpus[i].lapic_id == (uint8_t)id) return i;
    }
    return 0;
}
