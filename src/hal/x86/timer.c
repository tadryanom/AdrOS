#include "hal/timer.h"

#if defined(__i386__)
#include "arch/x86/idt.h"
#include "arch/x86/lapic.h"
#include "arch/x86/ioapic.h"
#include "io.h"
#include "console.h"

static hal_timer_tick_cb_t g_tick_cb = 0;

static void timer_irq(struct registers* regs) {
    (void)regs;
    if (lapic_is_enabled() && lapic_get_id() != 0) {
        /* AP: only run the local scheduler — tick accounting, VGA flush,
         * UART poll, and sleep-queue wake are handled by the BSP. */
        extern void schedule(void);
        schedule();
        return;
    }
    if (g_tick_cb) g_tick_cb();
}

void hal_timer_init(uint32_t frequency_hz, hal_timer_tick_cb_t tick_cb) {
    g_tick_cb = tick_cb;

    register_interrupt_handler(32, timer_irq);

    if (lapic_is_enabled()) {
        /* Use LAPIC timer — more precise and per-CPU capable.
         * Mask PIT IRQ 0 via IOAPIC so only the LAPIC timer drives
         * vector 32.  Without this, PIT adds ~18 extra ticks/sec,
         * making all timing calculations off by ~18%. */
        ioapic_mask_irq(0);
        lapic_timer_start(frequency_hz);
    } else {
        /* Fallback to legacy PIT */
        uint32_t divisor = 1193180 / frequency_hz;
        outb(0x43, 0x36);
        uint8_t l = (uint8_t)(divisor & 0xFF);
        uint8_t h = (uint8_t)((divisor >> 8) & 0xFF);
        outb(0x40, l);
        outb(0x40, h);
    }
}
#else
void hal_timer_init(uint32_t frequency_hz, hal_timer_tick_cb_t tick_cb) {
    (void)frequency_hz;
    (void)tick_cb;
}
#endif
