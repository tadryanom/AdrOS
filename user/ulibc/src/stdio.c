#include "stdio.h"
#include "unistd.h"
#include "string.h"
#include <stdarg.h>

static FILE _stdin_file  = { .fd = 0, .flags = _STDIO_READ };
static FILE _stdout_file = { .fd = 1, .flags = _STDIO_WRITE };
static FILE _stderr_file = { .fd = 2, .flags = _STDIO_WRITE };

FILE* stdin  = &_stdin_file;
FILE* stdout = &_stdout_file;
FILE* stderr = &_stderr_file;

static FILE _file_pool[FOPEN_MAX];
static int _file_pool_used[FOPEN_MAX];

FILE* fopen(const char* path, const char* mode) {
    int flags = 0;
    int stdio_flags = 0;
    if (mode[0] == 'r') {
        flags = 0; /* O_RDONLY */
        stdio_flags = _STDIO_READ;
    } else if (mode[0] == 'w') {
        flags = 1 | 0x40 | 0x200; /* O_WRONLY | O_CREAT | O_TRUNC */
        stdio_flags = _STDIO_WRITE;
    } else if (mode[0] == 'a') {
        flags = 1 | 0x40 | 0x400; /* O_WRONLY | O_CREAT | O_APPEND */
        stdio_flags = _STDIO_WRITE;
    } else {
        return (FILE*)0;
    }
    int fd = open(path, flags);
    if (fd < 0) return (FILE*)0;
    for (int i = 0; i < FOPEN_MAX; i++) {
        if (!_file_pool_used[i]) {
            _file_pool_used[i] = 1;
            _file_pool[i].fd = fd;
            _file_pool[i].flags = stdio_flags;
            _file_pool[i].buf_pos = 0;
            _file_pool[i].buf_len = 0;
            return &_file_pool[i];
        }
    }
    close(fd);
    return (FILE*)0;
}

int fflush(FILE* fp) {
    if (!fp) return EOF;
    if ((fp->flags & _STDIO_WRITE) && fp->buf_pos > 0) {
        write(fp->fd, fp->buf, (size_t)fp->buf_pos);
        fp->buf_pos = 0;
    }
    return 0;
}

int fclose(FILE* fp) {
    if (!fp) return EOF;
    fflush(fp);
    int rc = close(fp->fd);
    for (int i = 0; i < FOPEN_MAX; i++) {
        if (&_file_pool[i] == fp) { _file_pool_used[i] = 0; break; }
    }
    return rc;
}

size_t fread(void* ptr, size_t size, size_t nmemb, FILE* fp) {
    if (!fp || !ptr || size == 0 || nmemb == 0) return 0;
    size_t total = size * nmemb;
    size_t done = 0;
    char* dst = (char*)ptr;
    while (done < total) {
        if (fp->buf_pos >= fp->buf_len) {
            int r = read(fp->fd, fp->buf, BUFSIZ);
            if (r <= 0) { fp->flags |= _STDIO_EOF; break; }
            fp->buf_pos = 0;
            fp->buf_len = r;
        }
        int avail = fp->buf_len - fp->buf_pos;
        int want = (int)(total - done);
        int chunk = avail < want ? avail : want;
        memcpy(dst + done, fp->buf + fp->buf_pos, (size_t)chunk);
        fp->buf_pos += chunk;
        done += (size_t)chunk;
    }
    return done / size;
}

size_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* fp) {
    if (!fp || !ptr || size == 0 || nmemb == 0) return 0;
    size_t total = size * nmemb;
    const char* src = (const char*)ptr;
    size_t done = 0;
    while (done < total) {
        int space = BUFSIZ - fp->buf_pos;
        int want = (int)(total - done);
        int chunk = space < want ? space : want;
        memcpy(fp->buf + fp->buf_pos, src + done, (size_t)chunk);
        fp->buf_pos += chunk;
        done += (size_t)chunk;
        if (fp->buf_pos >= BUFSIZ) fflush(fp);
    }
    return done / size;
}

int fgetc(FILE* fp) {
    unsigned char c;
    if (fread(&c, 1, 1, fp) != 1) return EOF;
    return (int)c;
}

char* fgets(char* s, int size, FILE* fp) {
    if (!s || size <= 0 || !fp) return (char*)0;
    int i = 0;
    while (i < size - 1) {
        int c = fgetc(fp);
        if (c == EOF) { if (i == 0) return (char*)0; break; }
        s[i++] = (char)c;
        if (c == '\n') break;
    }
    s[i] = '\0';
    return s;
}

int fputc(int c, FILE* fp) {
    unsigned char ch = (unsigned char)c;
    if (fwrite(&ch, 1, 1, fp) != 1) return EOF;
    return (int)ch;
}

int fputs(const char* s, FILE* fp) {
    size_t len = strlen(s);
    if (fwrite(s, 1, len, fp) != len) return EOF;
    return 0;
}

int feof(FILE* fp) { return fp ? (fp->flags & _STDIO_EOF) : 0; }
int ferror(FILE* fp) { return fp ? (fp->flags & _STDIO_ERR) : 0; }

int vfprintf(FILE* fp, const char* fmt, va_list ap) {
    char buf[1024];
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    if (n > 0) {
        int w = n;
        if (w > (int)(sizeof(buf) - 1)) w = (int)(sizeof(buf) - 1);
        fwrite(buf, 1, (size_t)w, fp);
    }
    return n;
}

int fprintf(FILE* fp, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vfprintf(fp, fmt, ap);
    va_end(ap);
    return r;
}

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
