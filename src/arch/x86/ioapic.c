#include "arch/x86/ioapic.h"
#include "arch/x86/lapic.h"
#include "kernel_va_map.h"
#include "vmm.h"
#include "console.h"
#include "utils.h"

#include <stdint.h>

static volatile uint32_t* ioapic_base = 0;
static int ioapic_active = 0;
static uint8_t ioapic_max_irqs = 0;

static uint32_t ioapic_read(uint32_t reg) {
    ioapic_base[IOAPIC_REGSEL / 4] = reg;
    return ioapic_base[IOAPIC_REGWIN / 4];
}

static void ioapic_write(uint32_t reg, uint32_t val) {
    ioapic_base[IOAPIC_REGSEL / 4] = reg;
    ioapic_base[IOAPIC_REGWIN / 4] = val;
}

int ioapic_is_enabled(void) {
    return ioapic_active;
}

int ioapic_init(void) {
    if (!lapic_is_enabled()) {
        kprintf("[IOAPIC] LAPIC not enabled, skipping IOAPIC.\n");
        return 0;
    }

    /* Map IOAPIC MMIO region.
     * Default base is 0xFEC00000. In the future, ACPI MADT will provide this. */
    uintptr_t phys_base = IOAPIC_DEFAULT_BASE;
    uintptr_t ioapic_va = KVA_IOAPIC;

    vmm_map_page((uint64_t)phys_base, (uint64_t)ioapic_va,
                 VMM_FLAG_PRESENT | VMM_FLAG_RW | VMM_FLAG_NOCACHE);
    ioapic_base = (volatile uint32_t*)ioapic_va;

    /* Read IOAPIC version to get max redirection entries */
    uint32_t ver = ioapic_read(IOAPIC_REG_VER);
    ioapic_max_irqs = (uint8_t)(((ver >> 16) & 0xFF) + 1);
    if (ioapic_max_irqs > IOAPIC_MAX_IRQS) {
        ioapic_max_irqs = IOAPIC_MAX_IRQS;
    }

    /* Mask all IRQ lines initially */
    for (uint8_t i = 0; i < ioapic_max_irqs; i++) {
        ioapic_mask_irq(i);
    }

    ioapic_active = 1;

    kprintf("[IOAPIC] Enabled at phys=0x%x, max IRQs=%u\n",
            (unsigned)phys_base, (unsigned)ioapic_max_irqs);

    return 1;
}

void ioapic_route_irq(uint8_t irq, uint8_t vector, uint8_t lapic_id) {
    if (!ioapic_active) return;
    if (irq >= ioapic_max_irqs) return;

    uint32_t reg_lo = IOAPIC_REG_REDTBL + (uint32_t)irq * 2;
    uint32_t reg_hi = reg_lo + 1;

    /* High 32 bits: destination LAPIC ID in bits 24-31 */
    ioapic_write(reg_hi, (uint32_t)lapic_id << 24);

    /* Low 32 bits: vector, physical destination, edge-triggered, active-high, unmasked */
    uint32_t lo = (uint32_t)vector;  /* vector in bits 0-7 */
    /* bits 8-10: delivery mode = 000 (Fixed) */
    /* bit 11: destination mode = 0 (Physical) */
    /* bit 13: pin polarity = 0 (Active High) */
    /* bit 15: trigger mode = 0 (Edge) */
    /* bit 16: mask = 0 (Unmasked) */
    ioapic_write(reg_lo, lo);
}

void ioapic_route_irq_level(uint8_t irq, uint8_t vector, uint8_t lapic_id) {
    if (!ioapic_active) return;
    if (irq >= ioapic_max_irqs) return;

    uint32_t reg_lo = IOAPIC_REG_REDTBL + (uint32_t)irq * 2;
    uint32_t reg_hi = reg_lo + 1;

    /* High 32 bits: destination LAPIC ID in bits 24-31 */
    ioapic_write(reg_hi, (uint32_t)lapic_id << 24);

    /* Low 32 bits: vector, physical destination, level-triggered, active-low, unmasked */
    uint32_t lo = (uint32_t)vector;
    lo |= IOAPIC_RED_ACTIVELO; /* bit 13: active-low (PCI spec) */
    lo |= IOAPIC_RED_LEVEL;   /* bit 15: level-triggered (PCI spec) */
    ioapic_write(reg_lo, lo);
}

void ioapic_mask_irq(uint8_t irq) {
    if (!ioapic_active && ioapic_base == 0) return;
    if (irq >= IOAPIC_MAX_IRQS) return;

    uint32_t reg_lo = IOAPIC_REG_REDTBL + (uint32_t)irq * 2;
    uint32_t val = ioapic_read(reg_lo);
    ioapic_write(reg_lo, val | IOAPIC_RED_MASKED);
}

void ioapic_unmask_irq(uint8_t irq) {
    if (!ioapic_active) return;
    if (irq >= ioapic_max_irqs) return;

    uint32_t reg_lo = IOAPIC_REG_REDTBL + (uint32_t)irq * 2;
    uint32_t val = ioapic_read(reg_lo);
    ioapic_write(reg_lo, val & ~IOAPIC_RED_MASKED);
}
