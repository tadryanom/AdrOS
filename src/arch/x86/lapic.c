#include "arch/x86/lapic.h"
#include "hal/cpu_features.h"
#include "vmm.h"
#include "io.h"
#include "console.h"
#include "utils.h"

#include <stdint.h>

/* IA32_APIC_BASE MSR */
#define IA32_APIC_BASE_MSR     0x1B
#define IA32_APIC_BASE_ENABLE  (1U << 11)
#define IA32_APIC_BASE_ADDR    0xFFFFF000U

static volatile uint32_t* lapic_base = 0;
static int lapic_active = 0;

uint64_t rdmsr(uint32_t msr) {
    uint32_t lo, hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}

void wrmsr(uint32_t msr, uint64_t value) {
    uint32_t lo = (uint32_t)(value & 0xFFFFFFFF);
    uint32_t hi = (uint32_t)(value >> 32);
    __asm__ volatile("wrmsr" : : "c"(msr), "a"(lo), "d"(hi));
}

uint32_t lapic_read(uint32_t reg) {
    return lapic_base[reg / 4];
}

void lapic_write(uint32_t reg, uint32_t val) {
    lapic_base[reg / 4] = val;
}

int lapic_is_enabled(void) {
    return lapic_active;
}

uint32_t lapic_get_id(void) {
    if (!lapic_active) return 0;
    return (lapic_read(LAPIC_ID) >> 24) & 0xFF;
}

void lapic_eoi(void) {
    if (lapic_active) {
        lapic_write(LAPIC_EOI, 0);
    }
}

void lapic_send_ipi(uint8_t dest_id, uint32_t icr_lo) {
    if (!lapic_active) return;
    /* Write destination LAPIC ID to ICR high (bits 24-31) */
    lapic_write(LAPIC_ICR_HI, ((uint32_t)dest_id) << 24);
    /* Write command to ICR low — this triggers the IPI */
    lapic_write(LAPIC_ICR_LO, icr_lo);
    /* Wait for delivery (bit 12 = delivery status, 0 = idle) */
    while (lapic_read(LAPIC_ICR_LO) & (1U << 12)) {
        __asm__ volatile("pause");
    }
}

/* Disable the legacy 8259 PIC by masking all IRQs */
void pic_disable(void) {
    outb(0xA1, 0xFF);  /* Mask all slave PIC IRQs */
    outb(0x21, 0xFF);  /* Mask all master PIC IRQs */
}

int lapic_init(void) {
    const struct cpu_features* f = hal_cpu_get_features();
    if (!f->has_apic) {
        kprintf("[LAPIC] CPU does not support APIC.\n");
        return 0;
    }
    if (!f->has_msr) {
        kprintf("[LAPIC] CPU does not support MSR.\n");
        return 0;
    }

    /* Read APIC base address from MSR */
    uint64_t apic_base_msr = rdmsr(IA32_APIC_BASE_MSR);
    uintptr_t phys_base = (uintptr_t)(apic_base_msr & IA32_APIC_BASE_ADDR);

    /* Enable APIC in MSR if not already enabled */
    if (!(apic_base_msr & IA32_APIC_BASE_ENABLE)) {
        apic_base_msr |= IA32_APIC_BASE_ENABLE;
        wrmsr(IA32_APIC_BASE_MSR, apic_base_msr);
    }

    /* Map LAPIC MMIO region into kernel virtual address space.
     * Use a fixed kernel VA for the LAPIC page. */
    uintptr_t lapic_va = 0xC0400000U;  /* Fixed kernel VA, well above _end */
    vmm_map_page((uint64_t)phys_base, (uint64_t)lapic_va,
                 VMM_FLAG_PRESENT | VMM_FLAG_RW | VMM_FLAG_NOCACHE);
    lapic_base = (volatile uint32_t*)lapic_va;

    /* NOTE: Do NOT disable the PIC here. The PIC must remain active
     * until the IOAPIC is fully configured with IRQ routes.
     * The caller (arch_platform_setup) will disable the PIC after
     * IOAPIC setup is complete. */

    /* Set spurious interrupt vector and enable APIC */
    lapic_write(LAPIC_SVR, LAPIC_SVR_ENABLE | LAPIC_SPURIOUS_VEC);

    /* Clear error status register (write twice per Intel spec) */
    lapic_write(LAPIC_ESR, 0);
    lapic_write(LAPIC_ESR, 0);

    /* Set task priority to 0 (accept all interrupts) */
    lapic_write(LAPIC_TPR, 0);

    /* Mask LINT0 and LINT1 */
    lapic_write(LAPIC_LINT0_LVT, LAPIC_LVT_MASKED);
    lapic_write(LAPIC_LINT1_LVT, LAPIC_LVT_MASKED);

    /* Mask timer initially */
    lapic_write(LAPIC_TIMER_LVT, LAPIC_LVT_MASKED);

    /* Send EOI to clear any pending interrupts */
    lapic_eoi();

    lapic_active = 1;

    kprintf("[LAPIC] Enabled at phys=0x%x, ID=%u\n",
            (unsigned)phys_base, (unsigned)lapic_get_id());

    return 1;
}

void lapic_timer_start(uint32_t frequency_hz) {
    if (!lapic_active) return;

    /* Use divide-by-16 */
    lapic_write(LAPIC_TIMER_DCR, LAPIC_TIMER_DIV_16);

    /* Calibrate: use PIT channel 2 to measure LAPIC timer speed.
     * We'll use a simple approach: count how many LAPIC ticks in ~10ms.
     *
     * PIT channel 2 at 1193180 Hz, count for ~10ms = 11932 ticks.
     */
    /* Set PIT channel 2 for one-shot, mode 0 */
    outb(0x61, (inb(0x61) & 0xFD) | 0x01);  /* Gate high, speaker off */
    outb(0x43, 0xB0);                         /* Channel 2, lobyte/hibyte, mode 0 */
    uint16_t pit_count = 11932;               /* ~10ms */
    outb(0x42, (uint8_t)(pit_count & 0xFF));
    outb(0x42, (uint8_t)((pit_count >> 8) & 0xFF));

    /* Reset LAPIC timer to max count */
    lapic_write(LAPIC_TIMER_ICR, 0xFFFFFFFF);

    /* Wait for PIT to count down */
    while (!(inb(0x61) & 0x20)) {
        __asm__ volatile("pause");
    }

    /* Stop LAPIC timer */
    lapic_write(LAPIC_TIMER_LVT, LAPIC_LVT_MASKED);

    /* Read how many ticks elapsed in ~10ms */
    uint32_t elapsed = 0xFFFFFFFF - lapic_read(LAPIC_TIMER_CCR);

    /* Calculate ticks per desired frequency:
     * ticks_per_second = elapsed * 100 (since we measured 10ms)
     * ticks_per_interrupt = ticks_per_second / frequency_hz */
    uint32_t ticks_per_interrupt = (elapsed * 100) / frequency_hz;

    if (ticks_per_interrupt == 0) ticks_per_interrupt = 1;

    /* Configure periodic timer */
    lapic_write(LAPIC_TIMER_LVT, LAPIC_TIMER_PERIODIC | LAPIC_TIMER_VEC);
    lapic_write(LAPIC_TIMER_DCR, LAPIC_TIMER_DIV_16);
    lapic_write(LAPIC_TIMER_ICR, ticks_per_interrupt);

    kprintf("[LAPIC] Timer started at %uHz (ticks=0x%x)\n",
            (unsigned)frequency_hz, ticks_per_interrupt);
}

void lapic_timer_stop(void) {
    if (!lapic_active) return;
    lapic_write(LAPIC_TIMER_LVT, LAPIC_LVT_MASKED);
    lapic_write(LAPIC_TIMER_ICR, 0);
}
