#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

#include "console.h"

#include "spinlock.h"
#include "uart_console.h"
#include "hal/uart.h"
#include "hal/cpu.h"
#include "vga_console.h"
#include "keyboard.h"

static spinlock_t g_console_lock = {0};
static int g_console_uart_enabled = 1;
static int g_console_vga_enabled = 0;

/* ---- Kernel log ring buffer (like Linux __log_buf) ---- */
#define KLOG_BUF_SIZE 16384
static char klog_buf[KLOG_BUF_SIZE];
static size_t klog_head = 0;   // next write position
static size_t klog_count = 0;  // total bytes stored (capped at KLOG_BUF_SIZE)
static spinlock_t klog_lock = {0};

static void klog_append(const char* s, size_t len) {
    for (size_t i = 0; i < len; i++) {
        klog_buf[klog_head] = s[i];
        klog_head = (klog_head + 1) % KLOG_BUF_SIZE;
    }
    klog_count += len;
    if (klog_count > KLOG_BUF_SIZE) {
        klog_count = KLOG_BUF_SIZE;
    }
}

void console_init(void) {
    spinlock_init(&g_console_lock);
    g_console_uart_enabled = 1;
    g_console_vga_enabled = 0;
}

void console_enable_uart(int enabled) {
    uintptr_t flags = spin_lock_irqsave(&g_console_lock);
    g_console_uart_enabled = enabled ? 1 : 0;
    spin_unlock_irqrestore(&g_console_lock, flags);
}

void console_enable_vga(int enabled) {
    uintptr_t flags = spin_lock_irqsave(&g_console_lock);
    g_console_vga_enabled = enabled ? 1 : 0;
    spin_unlock_irqrestore(&g_console_lock, flags);
}

void console_write(const char* s) {
    if (!s) return;

    uintptr_t flags = spin_lock_irqsave(&g_console_lock);

    if (g_console_uart_enabled) {
        uart_print(s);
    }
    if (g_console_vga_enabled) {
        vga_print(s);
    }

    spin_unlock_irqrestore(&g_console_lock, flags);
}

void console_put_char(char c) {
    uintptr_t flags = spin_lock_irqsave(&g_console_lock);

    if (g_console_uart_enabled) {
        hal_uart_putc(c);
    }
    if (g_console_vga_enabled) {
        vga_put_char(c);
    }

    spin_unlock_irqrestore(&g_console_lock, flags);
}

static void out_char(char** dst, size_t* rem, int* total, char c) {
    (*total)++;
    if (*rem == 0) return;
    **dst = c;
    (*dst)++;
    (*rem)--;
}

static void out_str(char** dst, size_t* rem, int* total, const char* s) {
    if (!s) s = "(null)";
    for (size_t i = 0; s[i] != 0; i++) {
        out_char(dst, rem, total, s[i]);
    }
}

static void out_uint_base_u32(char** dst, size_t* rem, int* total, uint32_t v, unsigned base, int upper) {
    char tmp[32];
    size_t n = 0;

    if (base < 2 || base > 16) base = 10;

    if (v == 0) {
        tmp[n++] = '0';
    } else {
        while (v != 0 && n < sizeof(tmp)) {
            unsigned d = (unsigned)(v % (uint32_t)base);
            if (d < 10) tmp[n++] = (char)('0' + d);
            else tmp[n++] = (char)((upper ? 'A' : 'a') + (d - 10));
            v /= (uint32_t)base;
        }
    }

    while (n > 0) {
        out_char(dst, rem, total, tmp[--n]);
    }
}

static void out_uint_hex_fixed_u32(char** dst, size_t* rem, int* total, uint32_t v, int digits, int upper) {
    const char* hex = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    for (int i = digits - 1; i >= 0; i--) {
        unsigned nibble = (unsigned)((v >> (i * 4)) & 0xF);
        out_char(dst, rem, total, hex[nibble]);
    }
}

static void out_int_i32(char** dst, size_t* rem, int* total, int32_t v) {
    if (v < 0) {
        out_char(dst, rem, total, '-');
        out_uint_base_u32(dst, rem, total, (uint32_t)(-v), 10, 0);
    } else {
        out_uint_base_u32(dst, rem, total, (uint32_t)v, 10, 0);
    }
}

int kvsnprintf(char* out, size_t out_size, const char* fmt, va_list ap) {
    if (!out || out_size == 0) return 0;
    if (!fmt) {
        out[0] = 0;
        return 0;
    }

    char* dst = out;
    size_t rem = out_size - 1;
    int total = 0;

    for (size_t i = 0; fmt[i] != 0; i++) {
        char c = fmt[i];
        if (c != '%') {
            out_char(&dst, &rem, &total, c);
            continue;
        }

        char spec = fmt[++i];
        if (spec == 0) break;

        switch (spec) {
            case '%':
                out_char(&dst, &rem, &total, '%');
                break;
            case 'c': {
                int v = va_arg(ap, int);
                out_char(&dst, &rem, &total, (char)v);
                break;
            }
            case 's': {
                const char* s = va_arg(ap, const char*);
                out_str(&dst, &rem, &total, s);
                break;
            }
            case 'd':
            case 'i': {
                int v = va_arg(ap, int);
                out_int_i32(&dst, &rem, &total, (int32_t)v);
                break;
            }
            case 'u': {
                unsigned v = va_arg(ap, unsigned);
                out_uint_base_u32(&dst, &rem, &total, (uint32_t)v, 10, 0);
                break;
            }
            case 'x': {
                unsigned v = va_arg(ap, unsigned);
                out_uint_base_u32(&dst, &rem, &total, (uint32_t)v, 16, 0);
                break;
            }
            case 'X': {
                unsigned v = va_arg(ap, unsigned);
                out_uint_base_u32(&dst, &rem, &total, (uint32_t)v, 16, 1);
                break;
            }
            case 'p': {
                uintptr_t v = (uintptr_t)va_arg(ap, void*);
                out_str(&dst, &rem, &total, "0x");
                out_uint_hex_fixed_u32(&dst, &rem, &total, (uint32_t)v, 8, 1);
                break;
            }
            default:
                out_char(&dst, &rem, &total, '%');
                out_char(&dst, &rem, &total, spec);
                break;
        }
    }

    *dst = 0;
    return total;
}

int ksnprintf(char* out, size_t out_size, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = kvsnprintf(out, out_size, fmt, ap);
    va_end(ap);
    return r;
}

void kprintf(const char* fmt, ...) {
    char buf[512];

    va_list ap;
    va_start(ap, fmt);
    int len = kvsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    if (len > 0) {
        size_t slen = (size_t)len;
        if (slen >= sizeof(buf)) slen = sizeof(buf) - 1;

        uintptr_t lf = spin_lock_irqsave(&klog_lock);
        klog_append(buf, slen);
        spin_unlock_irqrestore(&klog_lock, lf);
    }

    console_write(buf);
}

int kgetc(void) {
    for (;;) {
        char c = 0;
        int rd = keyboard_read_nonblock(&c, 1);
        if (rd > 0) return (int)(unsigned char)c;

        int sc = hal_uart_try_getc();
        if (sc >= 0) return sc;

        hal_cpu_idle();
    }
}

size_t klog_read(char* out, size_t out_size) {
    if (!out || out_size == 0) return 0;

    uintptr_t flags = spin_lock_irqsave(&klog_lock);

    size_t avail = klog_count;
    if (avail > out_size - 1) avail = out_size - 1;

    size_t start;
    if (klog_count >= KLOG_BUF_SIZE) {
        start = klog_head; // oldest byte is right after head
    } else {
        start = (klog_head + KLOG_BUF_SIZE - klog_count) % KLOG_BUF_SIZE;
    }

    // skip oldest bytes if avail < klog_count
    size_t skip = klog_count - avail;
    start = (start + skip) % KLOG_BUF_SIZE;

    for (size_t i = 0; i < avail; i++) {
        out[i] = klog_buf[(start + i) % KLOG_BUF_SIZE];
    }
    out[avail] = '\0';

    spin_unlock_irqrestore(&klog_lock, flags);
    return avail;
}
