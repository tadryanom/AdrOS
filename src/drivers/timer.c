#include "timer.h"
#include "uart_console.h"
#include "process.h" 
#include "vdso.h"

#include "hal/timer.h"

static uint32_t tick = 0;

uint32_t get_tick_count(void) {
    return tick;
}

static void hal_tick_bridge(void) {
    tick++;
    vdso_update_tick(tick);
    process_wake_check(tick);
    schedule();
}

void timer_init(uint32_t frequency) {
    uart_print("[TIMER] Initializing...\n");
    hal_timer_init(frequency, hal_tick_bridge);
}
