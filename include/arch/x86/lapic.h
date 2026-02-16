#ifndef ARCH_X86_LAPIC_H
#define ARCH_X86_LAPIC_H

#include <stdint.h>

/* LAPIC register offsets (from LAPIC base) */
#define LAPIC_ID          0x020
#define LAPIC_VERSION     0x030
#define LAPIC_TPR         0x080  /* Task Priority Register */
#define LAPIC_EOI         0x0B0  /* End of Interrupt */
#define LAPIC_SVR         0x0F0  /* Spurious Interrupt Vector Register */
#define LAPIC_ESR         0x280  /* Error Status Register */
#define LAPIC_ICR_LO      0x300  /* Interrupt Command Register (low) */
#define LAPIC_ICR_HI      0x310  /* Interrupt Command Register (high) */
#define LAPIC_TIMER_LVT   0x320  /* LVT Timer Register */
#define LAPIC_LINT0_LVT   0x350  /* LVT LINT0 */
#define LAPIC_LINT1_LVT   0x360  /* LVT LINT1 */
#define LAPIC_TIMER_ICR   0x380  /* Timer Initial Count Register */
#define LAPIC_TIMER_CCR   0x390  /* Timer Current Count Register */
#define LAPIC_TIMER_DCR   0x3E0  /* Timer Divide Configuration Register */

/* SVR bits */
#define LAPIC_SVR_ENABLE  0x100  /* APIC Software Enable */
#define LAPIC_SVR_VECTOR  0xFF   /* Spurious vector number */

/* LVT Timer modes */
#define LAPIC_TIMER_ONESHOT   0x00000000
#define LAPIC_TIMER_PERIODIC  0x00020000
#define LAPIC_LVT_MASKED      0x00010000

/* Timer divide values for DCR */
#define LAPIC_TIMER_DIV_16    0x03

/* Spurious vector — pick an unused IDT slot */
#define LAPIC_SPURIOUS_VEC    0xFF

/* LAPIC timer IRQ vector — we use IDT slot 32 (same as PIT was) */
#define LAPIC_TIMER_VEC       32

/* IPI reschedule vector — sent to wake an idle AP when work arrives */
#define IPI_RESCHED_VEC       0xFD

/* Initialize the Local APIC. Returns 1 if APIC enabled, 0 if not available. */
int lapic_init(void);

/* Send End-of-Interrupt to the LAPIC. Must be called at end of every LAPIC interrupt. */
void lapic_eoi(void);

/* Read LAPIC register */
uint32_t lapic_read(uint32_t reg);

/* Write LAPIC register */
void lapic_write(uint32_t reg, uint32_t val);

/* Get the LAPIC ID of the current CPU */
uint32_t lapic_get_id(void);

/* Start the LAPIC timer at the given frequency (approximate). */
void lapic_timer_start(uint32_t frequency_hz);

/* Start LAPIC timer on an AP using BSP-calibrated ticks (no PIT recalibration). */
void lapic_timer_start_ap(void);

/* Stop the LAPIC timer. */
void lapic_timer_stop(void);

/* Returns 1 if LAPIC is enabled and active. */
int lapic_is_enabled(void);

/* Disable the legacy 8259 PIC by masking all IRQ lines.
 * Call AFTER IOAPIC is fully configured with IRQ routes. */
void pic_disable(void);

/* Send an IPI (Inter-Processor Interrupt) to a specific LAPIC.
 * dest_id: target LAPIC ID (placed in ICR_HI bits 24-31)
 * icr_lo:  ICR low word (delivery mode, vector, etc.) */
void lapic_send_ipi(uint8_t dest_id, uint32_t icr_lo);

/* MSR access helpers (defined in lapic.c) */
uint64_t rdmsr(uint32_t msr);
void wrmsr(uint32_t msr, uint64_t val);

#endif
