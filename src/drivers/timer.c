#include "timer.h"
#include "console.h"
#include "process.h"
#include "vdso.h"
#include "vga_console.h"

#include "hal/timer.h"
#include "hal/uart.h"

static uint32_t tick = 0;

/* TSC-based nanosecond timekeeping */
static uint32_t g_tsc_khz = 0;

uint32_t get_tick_count(void) {
    return tick;
}

void tsc_calibrate(uint32_t tsc_khz) {
    g_tsc_khz = tsc_khz;
}

uint32_t tsc_get_khz(void) {
    return g_tsc_khz;
}

static inline uint64_t rdtsc(void) {
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

static uint64_t g_tsc_boot = 0;

uint64_t clock_gettime_ns(void) {
    if (g_tsc_khz == 0) {
        /* Fallback: tick-based, 10ms granularity */
        uint64_t ms = (uint64_t)tick * TIMER_MS_PER_TICK;
        return ms * 1000000ULL;
    }

    uint64_t now = rdtsc();
    uint64_t delta = now - g_tsc_boot;
    /* ns = delta * 1000000 / tsc_khz
     * To avoid overflow on large deltas, split:
     * ns = (delta / tsc_khz) * 1000000 + ((delta % tsc_khz) * 1000000) / tsc_khz */
    uint64_t khz = (uint64_t)g_tsc_khz;
    uint64_t sec_part = (delta / khz) * 1000000ULL;
    uint64_t frac_part = ((delta % khz) * 1000000ULL) / khz;
    return sec_part + frac_part;
}

static void hal_tick_bridge(void) {
#ifdef __i386__
    extern uint32_t smp_current_cpu(void);
    uint32_t cpu = smp_current_cpu();
#else
    uint32_t cpu = 0;
#endif

    if (cpu == 0) {
        /* BSP: maintain tick counter, wake sleepers, flush display */
        tick++;
        vdso_update_tick(tick);
        vga_flush();
        hal_uart_poll_rx();
        process_wake_check(tick);
    }

    /* All CPUs: run the scheduler to pick up new work */
    schedule();
}

void timer_init(uint32_t frequency) {
    kprintf("[TIMER] Initializing...\n");
    g_tsc_boot = rdtsc();
    hal_timer_init(frequency, hal_tick_bridge);
}
