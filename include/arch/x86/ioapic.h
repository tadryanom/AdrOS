#ifndef ARCH_X86_IOAPIC_H
#define ARCH_X86_IOAPIC_H

#include <stdint.h>

/* IOAPIC register select / data window (MMIO) */
#define IOAPIC_REGSEL     0x00
#define IOAPIC_REGWIN     0x10

/* IOAPIC registers (via REGSEL) */
#define IOAPIC_REG_ID     0x00
#define IOAPIC_REG_VER    0x01
#define IOAPIC_REG_ARB    0x02
#define IOAPIC_REG_REDTBL 0x10  /* base; entry N = 0x10 + 2*N (lo), 0x11 + 2*N (hi) */

/* Redirection entry bits */
#define IOAPIC_RED_MASKED     (1U << 16)
#define IOAPIC_RED_LEVEL      (1U << 15)
#define IOAPIC_RED_ACTIVELO   (1U << 13)
#define IOAPIC_RED_LOGICAL    (1U << 11)

/* Default IOAPIC base address (can be overridden by ACPI MADT) */
#define IOAPIC_DEFAULT_BASE   0xFEC00000U

/* Maximum IRQ inputs on a standard IOAPIC */
#define IOAPIC_MAX_IRQS       24

/* Initialize the IOAPIC. Returns 1 on success, 0 if not available. */
int ioapic_init(void);

/* Route an ISA IRQ to a specific IDT vector, targeting a specific LAPIC ID.
 * irq: ISA IRQ number (0-15 typically)
 * vector: IDT vector number (32-255)
 * lapic_id: destination LAPIC ID */
void ioapic_route_irq(uint8_t irq, uint8_t vector, uint8_t lapic_id);

/* Mask (disable) an IRQ line on the IOAPIC. */
void ioapic_mask_irq(uint8_t irq);

/* Unmask (enable) an IRQ line on the IOAPIC. */
void ioapic_unmask_irq(uint8_t irq);

/* Returns 1 if IOAPIC is enabled and active. */
int ioapic_is_enabled(void);

#endif
