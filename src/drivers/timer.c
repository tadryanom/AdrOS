#include "timer.h"
#include "console.h"
#include "process.h" 
#include "vdso.h"
#include "vga_console.h"

#include "hal/timer.h"

#if defined(__i386__)
#include "arch/x86/lapic.h"
#endif

static uint32_t tick = 0;

uint32_t get_tick_count(void) {
    return tick;
}

static void hal_tick_bridge(void) {
#if defined(__i386__)
    if (lapic_is_enabled() && lapic_get_id() != 0) return;
#endif
    tick++;
    vdso_update_tick(tick);
    vga_flush();
    process_wake_check(tick);
    /* Preempt every SCHED_DIVISOR ticks to reduce context-switch
     * overhead in emulated environments (QEMU TLB flush on CR3
     * reload is expensive).  Sleeping processes still wake at full
     * TIMER_HZ resolution via process_wake_check above. */
    if (tick % 2 == 0)
        schedule();
}

void timer_init(uint32_t frequency) {
    kprintf("[TIMER] Initializing...\n");
    hal_timer_init(frequency, hal_tick_bridge);
}
