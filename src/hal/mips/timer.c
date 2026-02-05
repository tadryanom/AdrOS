#include "hal/timer.h"

void hal_timer_init(uint32_t frequency_hz, hal_timer_tick_cb_t tick_cb) {
    (void)frequency_hz;
    (void)tick_cb;
}
