#include "arch/x86/smp.h"
#include "arch/x86/acpi.h"
#include "arch/x86/lapic.h"
#include "arch/x86/idt.h"
#include "arch/x86/gdt.h"
#include "uart_console.h"
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

/* Per-AP kernel stack size */
#define AP_STACK_SIZE       4096U

#define KERNEL_VIRT_BASE    0xC0000000U

static struct cpu_info g_cpus[SMP_MAX_CPUS];
static volatile uint32_t g_cpu_count = 0;

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

    /* Find our cpu_info slot and mark started */
    for (uint32_t i = 0; i < g_cpu_count; i++) {
        if (g_cpus[i].lapic_id == (uint8_t)my_id) {
            __atomic_store_n(&g_cpus[i].started, 1, __ATOMIC_SEQ_CST);
            break;
        }
    }

    /* AP is now idle — halt until needed.
     * In the future, each AP will run its own scheduler. */
    for (;;) {
        __asm__ volatile("sti; hlt");
    }
}

int smp_init(void) {
    const struct acpi_info* acpi = acpi_get_info();
    if (!acpi || acpi->num_cpus <= 1) {
        g_cpu_count = 1;
        g_cpus[0].lapic_id = (uint8_t)lapic_get_id();
        g_cpus[0].cpu_index = 0;
        g_cpus[0].started = 1;
        uart_print("[SMP] Single CPU, no APs to start.\n");
        return 1;
    }

    /* Populate cpu_info from ACPI */
    g_cpu_count = acpi->num_cpus;
    uint8_t bsp_id = (uint8_t)lapic_get_id();

    for (uint8_t i = 0; i < acpi->num_cpus && i < SMP_MAX_CPUS; i++) {
        g_cpus[i].lapic_id = acpi->cpu_lapic_ids[i];
        g_cpus[i].cpu_index = i;
        g_cpus[i].started = (g_cpus[i].lapic_id == bsp_id) ? 1 : 0;
        g_cpus[i].kernel_stack = (uint32_t)(uintptr_t)&ap_stacks[i][AP_STACK_SIZE];
    }

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

    uart_print("[SMP] Starting ");
    char tmp[12];
    itoa(g_cpu_count - 1, tmp, 10);
    uart_print(tmp);
    uart_print(" AP(s)...\n");

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
            uart_print("[SMP] CPU ");
            itoa(g_cpus[i].lapic_id, tmp, 10);
            uart_print(tmp);
            uart_print(" started.\n");
        } else {
            uart_print("[SMP] CPU ");
            itoa(g_cpus[i].lapic_id, tmp, 10);
            uart_print(tmp);
            uart_print(" failed to start!\n");
        }
    }

    uint32_t started = 0;
    for (uint32_t i = 0; i < g_cpu_count; i++) {
        if (g_cpus[i].started) started++;
    }

    uart_print("[SMP] ");
    itoa(started, tmp, 10);
    uart_print(tmp);
    uart_print(" CPU(s) active.\n");

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
