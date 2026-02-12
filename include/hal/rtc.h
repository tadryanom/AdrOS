#ifndef HAL_RTC_H
#define HAL_RTC_H

#include <stdint.h>

/*
 * HAL RTC interface — architecture-specific implementations provide
 * raw register reads from the hardware real-time clock.
 *
 * All values are returned as raw hardware values (may be BCD-encoded
 * depending on the platform).  The generic driver layer handles
 * BCD-to-binary conversion and timestamp calculation.
 */

struct hal_rtc_raw {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint8_t year;       /* two-digit year from hardware */
    uint8_t status_b;   /* status/config register (x86: CMOS reg 0x0B) */
};

void hal_rtc_init(void);
void hal_rtc_read_raw(struct hal_rtc_raw* raw);

#endif
