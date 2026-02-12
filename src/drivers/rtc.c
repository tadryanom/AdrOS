#include "rtc.h"
#include "hal/rtc.h"

static uint8_t bcd_to_bin(uint8_t v) {
    return (uint8_t)((v & 0x0F) + ((v >> 4) * 10));
}

void rtc_init(void) {
    hal_rtc_init();
}

void rtc_read(struct rtc_time* t) {
    struct hal_rtc_raw raw;
    hal_rtc_read_raw(&raw);

    t->second = raw.second;
    t->minute = raw.minute;
    t->hour   = raw.hour;
    t->day    = raw.day;
    t->month  = raw.month;
    t->year   = raw.year;

    int bcd = !(raw.status_b & 0x04);

    if (bcd) {
        t->second = bcd_to_bin(t->second);
        t->minute = bcd_to_bin(t->minute);
        t->hour   = bcd_to_bin((uint8_t)(t->hour & 0x7F));
        t->day    = bcd_to_bin(t->day);
        t->month  = bcd_to_bin(t->month);
        t->year   = bcd_to_bin((uint8_t)t->year);
    }

    t->year += 2000;

    if (!(raw.status_b & 0x02) && (raw.hour & 0x80)) {
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
