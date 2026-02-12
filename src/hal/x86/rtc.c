#include "hal/rtc.h"

#include "arch/x86/io.h"

#define CMOS_ADDR 0x70
#define CMOS_DATA 0x71

#define RTC_REG_SEC   0x00
#define RTC_REG_MIN   0x02
#define RTC_REG_HOUR  0x04
#define RTC_REG_DAY   0x07
#define RTC_REG_MON   0x08
#define RTC_REG_YEAR  0x09
#define RTC_REG_STATA 0x0A
#define RTC_REG_STATB 0x0B

static uint8_t cmos_read(uint8_t reg) {
    outb(CMOS_ADDR, reg);
    return inb(CMOS_DATA);
}

static int rtc_updating(void) {
    outb(CMOS_ADDR, RTC_REG_STATA);
    return inb(CMOS_DATA) & 0x80;
}

void hal_rtc_init(void) {
    /* Nothing to do — CMOS is always available on x86 */
}

void hal_rtc_read_raw(struct hal_rtc_raw* raw) {
    if (!raw) return;

    while (rtc_updating()) {}

    raw->second   = cmos_read(RTC_REG_SEC);
    raw->minute   = cmos_read(RTC_REG_MIN);
    raw->hour     = cmos_read(RTC_REG_HOUR);
    raw->day      = cmos_read(RTC_REG_DAY);
    raw->month    = cmos_read(RTC_REG_MON);
    raw->year     = cmos_read(RTC_REG_YEAR);
    raw->status_b = cmos_read(RTC_REG_STATB);
}
