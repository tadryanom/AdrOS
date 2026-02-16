#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

#define TIMER_HZ           100
#define TIMER_MS_PER_TICK  (1000 / TIMER_HZ)   /* 10 ms */

void timer_init(uint32_t frequency);
uint32_t get_tick_count(void);

/* High-resolution monotonic clock (nanoseconds since boot).
 * Uses TSC if calibrated, falls back to tick-based 10ms granularity. */
uint64_t clock_gettime_ns(void);

/* TSC calibration (called during LAPIC timer setup) */
void tsc_calibrate(uint32_t tsc_khz);
uint32_t tsc_get_khz(void);

#endif
