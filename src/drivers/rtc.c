#include "rtc.h"

#define CMOS_ADDR 0x70
#define CMOS_DATA 0x71

#define RTC_REG_SEC   0x00
#define RTC_REG_MIN   0x02
#define RTC_REG_HOUR  0x04
#define RTC_REG_DAY   0x07
#define RTC_REG_MON   0x08
#define RTC_REG_YEAR  0x09
#define RTC_REG_STATB 0x0B

static inline void port_outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t port_inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static uint8_t cmos_read(uint8_t reg) {
    port_outb(CMOS_ADDR, reg);
    return port_inb(CMOS_DATA);
}

static int rtc_updating(void) {
    port_outb(CMOS_ADDR, 0x0A);
    return port_inb(CMOS_DATA) & 0x80;
}

static uint8_t bcd_to_bin(uint8_t v) {
    return (uint8_t)((v & 0x0F) + ((v >> 4) * 10));
}

void rtc_init(void) {
    /* Nothing to do — CMOS is always available on x86 */
}

void rtc_read(struct rtc_time* t) {
    while (rtc_updating()) {}

    t->second = cmos_read(RTC_REG_SEC);
    t->minute = cmos_read(RTC_REG_MIN);
    t->hour   = cmos_read(RTC_REG_HOUR);
    t->day    = cmos_read(RTC_REG_DAY);
    t->month  = cmos_read(RTC_REG_MON);
    t->year   = cmos_read(RTC_REG_YEAR);

    uint8_t statb = cmos_read(RTC_REG_STATB);
    int bcd = !(statb & 0x04);

    if (bcd) {
        t->second = bcd_to_bin(t->second);
        t->minute = bcd_to_bin(t->minute);
        t->hour   = bcd_to_bin((uint8_t)(t->hour & 0x7F));
        t->day    = bcd_to_bin(t->day);
        t->month  = bcd_to_bin(t->month);
        t->year   = bcd_to_bin((uint8_t)t->year);
    }

    t->year += 2000;

    if (!(statb & 0x02) && (cmos_read(RTC_REG_HOUR) & 0x80)) {
        t->hour = (uint8_t)((t->hour + 12) % 24);
    }
}

static int is_leap(uint16_t y) {
    return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}

uint32_t rtc_unix_timestamp(void) {
    struct rtc_time t;
    rtc_read(&t);

    static const uint16_t mdays[12] = {
        0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
    };

    uint32_t y = t.year;
    uint32_t days = 0;

    for (uint32_t i = 1970; i < y; i++) {
        days += is_leap((uint16_t)i) ? 366 : 365;
    }

    days += mdays[t.month - 1];
    if (t.month > 2 && is_leap((uint16_t)y)) days++;
    days += t.day - 1;

    return days * 86400 + t.hour * 3600 + t.minute * 60 + t.second;
}
