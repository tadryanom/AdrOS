#include "hal/timer.h"

#if defined(__i386__)
#include "idt.h"
#include "io.h"

static hal_timer_tick_cb_t g_tick_cb = 0;

static void timer_irq(struct registers* regs) {
    (void)regs;
    if (g_tick_cb) g_tick_cb();
}

void hal_timer_init(uint32_t frequency_hz, hal_timer_tick_cb_t tick_cb) {
    g_tick_cb = tick_cb;

    register_interrupt_handler(32, timer_irq);

    uint32_t divisor = 1193180 / frequency_hz;
    outb(0x43, 0x36);
    uint8_t l = (uint8_t)(divisor & 0xFF);
    uint8_t h = (uint8_t)((divisor >> 8) & 0xFF);
    outb(0x40, l);
    outb(0x40, h);
}
#else
void hal_timer_init(uint32_t frequency_hz, hal_timer_tick_cb_t tick_cb) {
    (void)frequency_hz;
    (void)tick_cb;
}
#endif
