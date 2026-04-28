// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#include "stdio.h"
#include "unistd.h"
#include "string.h"
#include "stdlib.h"
#include <stdarg.h>

static FILE _stdin_file  = { .fd = 0, .flags = _STDIO_READ };
static FILE _stdout_file = { .fd = 1, .flags = _STDIO_WRITE | _STDIO_LBUF };
static FILE _stderr_file = { .fd = 2, .flags = _STDIO_WRITE | _STDIO_UNBUF };

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

    /* Unbuffered: write directly, bypass buffer */
    if (fp->flags & _STDIO_UNBUF) {
        while (done < total) {
            int w = write(fp->fd, src + done, total - done);
            if (w <= 0) break;
            done += (size_t)w;
        }
        return done / size;
    }

    while (done < total) {
        int space = BUFSIZ - fp->buf_pos;
        int want = (int)(total - done);
        int chunk = space < want ? space : want;
        memcpy(fp->buf + fp->buf_pos, src + done, (size_t)chunk);
        fp->buf_pos += chunk;
        done += (size_t)chunk;
        if (fp->buf_pos >= BUFSIZ) fflush(fp);
    }

    /* Line-buffered: flush if the written data contains a newline */
    if ((fp->flags & _STDIO_LBUF) && fp->buf_pos > 0) {
        for (size_t i = 0; i < total; i++) {
            if (src[i] == '\n') {
                fflush(fp);
                break;
            }
        }
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

int fseek(FILE* fp, long offset, int whence) {
    if (!fp) return -1;
    fflush(fp);
    fp->buf_pos = 0;
    fp->buf_len = 0;
    fp->flags &= ~_STDIO_EOF;
    int rc = lseek(fp->fd, (int)offset, whence);
    return (rc < 0) ? -1 : 0;
}

long ftell(FILE* fp) {
    if (!fp) return -1;
    long pos = (long)lseek(fp->fd, 0, 1 /* SEEK_CUR */);
    if (pos < 0) return -1;
    if (fp->flags & _STDIO_READ) {
        pos -= (long)(fp->buf_len - fp->buf_pos);
    }
    return pos;
}

void rewind(FILE* fp) {
    if (fp) fseek(fp, 0, 0 /* SEEK_SET */);
}

int remove(const char* path) {
    return unlink(path);
}

int setvbuf(FILE* fp, char* buf, int mode, size_t size) {
    (void)buf; (void)size; /* we use internal buffer only */
    if (!fp) return -1;
    fp->flags &= ~(_STDIO_LBUF | _STDIO_UNBUF);
    if (mode == 1 /* _IOLBF */) fp->flags |= _STDIO_LBUF;
    else if (mode == 2 /* _IONBF */) fp->flags |= _STDIO_UNBUF;
    /* _IOFBF (0): fully buffered — no extra flag needed */
    return 0;
}

void setbuf(FILE* fp, char* buf) {
    if (!buf) setvbuf(fp, (char*)0, 2 /* _IONBF */, 0);
    else setvbuf(fp, buf, 0 /* _IOFBF */, BUFSIZ);
}

int vfprintf(FILE* fp, const char* fmt, va_list ap) {
    char buf[4096];
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    if (n > 0) {
        int w = n;
        if (w > (int)(sizeof(buf) - 1)) {
            /* Output was truncated — write the full buffer and report total */
            fwrite(buf, 1, sizeof(buf) - 1, fp);
        } else {
            fwrite(buf, 1, (size_t)w, fp);
        }
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
    unsigned char ch = (unsigned char)c;
    fwrite(&ch, 1, 1, stdout);
    return c;
}

int puts(const char* s) {
    int len = (int)strlen(s);
    fwrite(s, 1, (size_t)len, stdout);
    fwrite("\n", 1, 1, stdout);
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

int sprintf(char* buf, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vsnprintf(buf, (size_t)0x7FFFFFFF, fmt, ap);
    va_end(ap);
    return r;
}

int snprintf(char* buf, size_t size, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vsnprintf(buf, size, fmt, ap);
    va_end(ap);
    return r;
}

int sscanf(const char* str, const char* fmt, ...) {
    /* Minimal sscanf: only supports %d and %s */
    va_list ap;
    va_start(ap, fmt);
    int count = 0;
    const char* s = str;
    const char* f = fmt;
    while (*f && *s) {
        if (*f == '%') {
            f++;
            if (*f == 'd' || *f == 'i') {
                f++;
                int* out = va_arg(ap, int*);
                int neg = 0;
                while (*s == ' ') s++;
                if (*s == '-') { neg = 1; s++; }
                else if (*s == '+') { s++; }
                if (*s < '0' || *s > '9') break;
                int val = 0;
                while (*s >= '0' && *s <= '9') { val = val * 10 + (*s - '0'); s++; }
                *out = neg ? -val : val;
                count++;
            } else if (*f == 's') {
                f++;
                char* out = va_arg(ap, char*);
                while (*s == ' ') s++;
                int i = 0;
                while (*s && *s != ' ' && *s != '\n' && *s != '\t') out[i++] = *s++;
                out[i] = '\0';
                count++;
            } else {
                break;
            }
        } else if (*f == ' ') {
            f++;
            while (*s == ' ') s++;
        } else {
            if (*f != *s) break;
            f++; s++;
        }
    }
    va_end(ap);
    return count;
}

int vprintf(const char* fmt, va_list ap) {
    return vfprintf(stdout, fmt, ap);
}

int printf(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vprintf(fmt, ap);
    va_end(ap);
    return r;
}

void clearerr(FILE* fp) {
    if (fp) fp->flags &= ~(_STDIO_EOF | _STDIO_ERR);
}

int ungetc(int c, FILE* fp) {
    if (!fp || c == EOF) return EOF;
    /* Simple 1-byte pushback: store in buf_pos-1 if possible */
    if (fp->buf_pos > 0) {
        fp->buf_pos--;
        fp->buf[fp->buf_pos] = (char)c;
    } else if (fp->buf_len < (int)sizeof(fp->buf)) {
        /* Shift buffer right by 1 */
        for (int i = fp->buf_len; i > 0; i--)
            fp->buf[i] = fp->buf[i - 1];
        fp->buf[0] = (char)c;
        fp->buf_len++;
    } else {
        return EOF;
    }
    fp->flags &= ~_STDIO_EOF;
    return (unsigned char)c;
}

int getc(FILE* fp) { return fgetc(fp); }
int putc(int c, FILE* fp) { return fputc(c, fp); }

ssize_t getdelim(char** lineptr, size_t* n, int delim, FILE* stream) {
    if (!lineptr || !n || !stream) { return -1; }

    if (!*lineptr || *n == 0) {
        *n = 128;
        *lineptr = (char*)malloc(*n);
        if (!*lineptr) return -1;
    }

    size_t pos = 0;
    int c;
    while ((c = fgetc(stream)) != EOF) {
        if (pos + 2 > *n) {
            size_t newn = *n * 2;
            char* tmp = (char*)realloc(*lineptr, newn);
            if (!tmp) return -1;
            *lineptr = tmp;
            *n = newn;
        }
        (*lineptr)[pos++] = (char)c;
        if (c == delim) break;
    }
    if (pos == 0 && c == EOF) return -1;
    (*lineptr)[pos] = '\0';
    return (ssize_t)pos;
}

ssize_t getline(char** lineptr, size_t* n, FILE* stream) {
    return getdelim(lineptr, n, '\n', stream);
}

FILE* fdopen(int fd, const char* mode) {
    if (fd < 0 || !mode) return (FILE*)0;
    FILE* fp = (FILE*)malloc(sizeof(FILE));
    if (!fp) return (FILE*)0;
    fp->fd = fd;
    fp->flags = 0;
    fp->buf_pos = 0;
    fp->buf_len = 0;
    if (mode[0] == 'r') fp->flags |= _STDIO_READ;
    if (mode[0] == 'w' || mode[0] == 'a') fp->flags |= _STDIO_WRITE;
    if (mode[1] == '+') fp->flags |= _STDIO_READ | _STDIO_WRITE;
    return fp;
}

int fileno(FILE* fp) {
    if (!fp) return -1;
    return fp->fd;
}

FILE* popen(const char* command, const char* type) {
    int fds[2];
    extern int pipe(int[2]);
    extern int fork(void);
    extern int execl(const char*, const char*, ...);
    extern int dup2(int, int);
    extern int close(int);
    extern void _exit(int);

    if (pipe(fds) < 0) return (FILE*)0;

    int pid = fork();
    if (pid < 0) {
        close(fds[0]);
        close(fds[1]);
        return (FILE*)0;
    }
    if (pid == 0) {
        /* Child */
        if (type[0] == 'r') {
            close(fds[0]);
            dup2(fds[1], 1);
            close(fds[1]);
        } else {
            close(fds[1]);
            dup2(fds[0], 0);
            close(fds[0]);
        }
        execl("/bin/sh", "sh", "-c", command, (char*)0);
        _exit(127);
    }
    /* Parent */
    if (type[0] == 'r') {
        close(fds[1]);
        return fdopen(fds[0], "r");
    } else {
        close(fds[0]);
        return fdopen(fds[1], "w");
    }
}

int pclose(FILE* fp) {
    extern int waitpid(int, int*, int);
    if (!fp) return -1;
    int fd = fileno(fp);
    (void)fd;
    fclose(fp);
    int status = 0;
    waitpid(-1, &status, 0);
    return status;
}

FILE* tmpfile(void) {
    static int tmpcount = 0;
    char name[32];
    extern int getpid(void);
    snprintf(name, sizeof(name), "/tmp/.tmpf_%d_%d", getpid(), tmpcount++);
    return fopen(name, "w+");
}

char* tmpnam(char* s) {
    static char buf[32];
    static int count = 0;
    snprintf(buf, sizeof(buf), "/tmp/tmp_%d", count++);
    if (s) { strcpy(s, buf); return s; }
    return buf;
}

int fscanf(FILE* fp, const char* fmt, ...) {
    /* Read a line, then delegate to sscanf */
    char line[512];
    if (!fgets(line, (int)sizeof(line), fp)) return EOF;
    va_list ap;
    va_start(ap, fmt);
    /* We can't pass va_list to sscanf directly, so use a manual approach
     * that matches sscanf's minimal implementation for %d and %s */
    int count = 0;
    const char* s = line;
    const char* f = fmt;
    while (*f && *s) {
        if (*f == '%') {
            f++;
            if (*f == 'd' || *f == 'i') {
                f++;
                int* out = va_arg(ap, int*);
                int neg = 0;
                while (*s == ' ') s++;
                if (*s == '-') { neg = 1; s++; }
                else if (*s == '+') { s++; }
                if (*s < '0' || *s > '9') break;
                int val = 0;
                while (*s >= '0' && *s <= '9') { val = val * 10 + (*s - '0'); s++; }
                *out = neg ? -val : val;
                count++;
            } else if (*f == 's') {
                f++;
                char* out = va_arg(ap, char*);
                while (*s == ' ') s++;
                int i = 0;
                while (*s && *s != ' ' && *s != '\n' && *s != '\t') out[i++] = *s++;
                out[i] = '\0';
                count++;
            } else if (*f == 'c') {
                f++;
                char* out = va_arg(ap, char*);
                *out = *s++;
                count++;
            } else {
                break;
            }
        } else if (*f == ' ') {
            f++;
            while (*s == ' ') s++;
        } else {
            if (*f != *s) break;
            f++; s++;
        }
    }
    va_end(ap);
    return count;
}

int scanf(const char* fmt, ...) {
    char line[512];
    if (!fgets(line, (int)sizeof(line), stdin)) return EOF;
    va_list ap;
    va_start(ap, fmt);
    int count = 0;
    const char* s = line;
    const char* f = fmt;
    while (*f && *s) {
        if (*f == '%') {
            f++;
            if (*f == 'd' || *f == 'i') {
                f++;
                int* out = va_arg(ap, int*);
                int neg = 0;
                while (*s == ' ') s++;
                if (*s == '-') { neg = 1; s++; }
                else if (*s == '+') { s++; }
                if (*s < '0' || *s > '9') break;
                int val = 0;
                while (*s >= '0' && *s <= '9') { val = val * 10 + (*s - '0'); s++; }
                *out = neg ? -val : val;
                count++;
            } else if (*f == 's') {
                f++;
                char* out = va_arg(ap, char*);
                while (*s == ' ') s++;
                int i = 0;
                while (*s && *s != ' ' && *s != '\n' && *s != '\t') out[i++] = *s++;
                out[i] = '\0';
                count++;
            } else {
                break;
            }
        } else if (*f == ' ') {
            f++;
            while (*s == ' ') s++;
        } else {
            if (*f != *s) break;
            f++; s++;
        }
    }
    va_end(ap);
    return count;
}
