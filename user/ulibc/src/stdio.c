#include "stdio.h"
#include "unistd.h"
#include "string.h"
#include <stdarg.h>

int putchar(int c) {
    char ch = (char)c;
    write(STDOUT_FILENO, &ch, 1);
    return c;
}

int puts(const char* s) {
    int len = (int)strlen(s);
    write(STDOUT_FILENO, s, (size_t)len);
    write(STDOUT_FILENO, "\n", 1);
    return len + 1;
}

/* Minimal vsnprintf supporting: %d %i %u %x %X %s %c %p %% */
int vsnprintf(char* buf, size_t size, const char* fmt, va_list ap) {
    size_t pos = 0;

#define PUTC(c) do { if (pos + 1 < size) buf[pos] = (c); pos++; } while(0)

    if (size == 0) {
        /* Cannot write anything; return 0 */
        return 0;
    }

    while (*fmt) {
        if (*fmt != '%') {
            PUTC(*fmt);
            fmt++;
            continue;
        }
        fmt++; /* skip '%' */

        /* Flags */
        int pad_zero = 0;
        int left_align = 0;
        while (*fmt == '0' || *fmt == '-') {
            if (*fmt == '0') pad_zero = 1;
            if (*fmt == '-') left_align = 1;
            fmt++;
        }
        if (left_align) pad_zero = 0;

        /* Width */
        int width = 0;
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }

        /* Specifier */
        char tmp[32];
        int tmplen = 0;
        const char* str = 0;

        switch (*fmt) {
        case 'd':
        case 'i': {
            int v = va_arg(ap, int);
            int neg = 0;
            unsigned int uv;
            if (v < 0) { neg = 1; uv = (unsigned int)(-(v + 1)) + 1; }
            else { uv = (unsigned int)v; }
            if (uv == 0) { tmp[tmplen++] = '0'; }
            else { while (uv) { tmp[tmplen++] = (char)('0' + uv % 10); uv /= 10; } }
            if (neg) tmp[tmplen++] = '-';
            /* reverse */
            for (int i = 0; i < tmplen / 2; i++) {
                char t = tmp[i]; tmp[i] = tmp[tmplen-1-i]; tmp[tmplen-1-i] = t;
            }
            str = tmp;
            break;
        }
        case 'u': {
            unsigned int v = va_arg(ap, unsigned int);
            if (v == 0) { tmp[tmplen++] = '0'; }
            else { while (v) { tmp[tmplen++] = (char)('0' + v % 10); v /= 10; } }
            for (int i = 0; i < tmplen / 2; i++) {
                char t = tmp[i]; tmp[i] = tmp[tmplen-1-i]; tmp[tmplen-1-i] = t;
            }
            str = tmp;
            break;
        }
        case 'x':
        case 'X':
        case 'p': {
            const char* hex = (*fmt == 'X') ? "0123456789ABCDEF" : "0123456789abcdef";
            unsigned int v;
            if (*fmt == 'p') {
                v = (unsigned int)(uintptr_t)va_arg(ap, void*);
                tmp[tmplen++] = '0';
                tmp[tmplen++] = 'x';
            } else {
                v = va_arg(ap, unsigned int);
            }
            int start = tmplen;
            if (v == 0) { tmp[tmplen++] = '0'; }
            else { while (v) { tmp[tmplen++] = hex[v & 0xF]; v >>= 4; } }
            /* reverse the hex digits only */
            for (int i = start; i < start + (tmplen - start) / 2; i++) {
                int j = start + (tmplen - start) - 1 - (i - start);
                char t = tmp[i]; tmp[i] = tmp[j]; tmp[j] = t;
            }
            str = tmp;
            break;
        }
        case 's': {
            str = va_arg(ap, const char*);
            if (!str) str = "(null)";
            tmplen = (int)strlen(str);
            break;
        }
        case 'c': {
            tmp[0] = (char)va_arg(ap, int);
            tmplen = 1;
            str = tmp;
            break;
        }
        case '%':
            PUTC('%');
            fmt++;
            continue;
        case '\0':
            goto done;
        default:
            PUTC('%');
            PUTC(*fmt);
            fmt++;
            continue;
        }
        fmt++;

        if (str && tmplen == 0) tmplen = (int)strlen(str);

        /* Padding */
        int pad = width - tmplen;
        if (!left_align && pad > 0) {
            char pc = pad_zero ? '0' : ' ';
            for (int i = 0; i < pad; i++) PUTC(pc);
        }
        for (int i = 0; i < tmplen; i++) PUTC(str[i]);
        if (left_align && pad > 0) {
            for (int i = 0; i < pad; i++) PUTC(' ');
        }
    }

done:
    if (pos < size) buf[pos] = '\0';
    else if (size > 0) buf[size - 1] = '\0';
    return (int)pos;

#undef PUTC
}

int snprintf(char* buf, size_t size, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vsnprintf(buf, size, fmt, ap);
    va_end(ap);
    return r;
}

int vprintf(const char* fmt, va_list ap) {
    char buf[1024];
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    if (n > 0) {
        int w = n;
        if (w > (int)(sizeof(buf) - 1)) w = (int)(sizeof(buf) - 1);
        write(STDOUT_FILENO, buf, (size_t)w);
    }
    return n;
}

int printf(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vprintf(fmt, ap);
    va_end(ap);
    return r;
}
