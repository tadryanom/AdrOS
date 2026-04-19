// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

/*
 * Newlib libgloss syscall stubs for AdrOS (i686)
 *
 * These functions implement the minimal OS interface that Newlib requires.
 * Each stub issues an AdrOS syscall via INT 0x80.
 *
 * AdrOS syscall convention (i386):
 *   EAX = syscall number
 *   EBX = arg1, ECX = arg2, EDX = arg3, ESI = arg4, EDI = arg5
 *   Return value in EAX (negative = -errno)
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <sys/time.h>
#include <errno.h>
#include <stdint.h>

#ifndef _CADDR_T
typedef char *caddr_t;
#endif

/* ---- AdrOS syscall numbers (must match include/syscall.h) ---- */
#define SYS_WRITE       1
#define SYS_EXIT        2
#define SYS_GETPID      3
#define SYS_OPEN        4
#define SYS_READ        5
#define SYS_CLOSE       6
#define SYS_WAITPID     7
#define SYS_LSEEK       9
#define SYS_FSTAT       10
#define SYS_STAT        11
#define SYS_EXECVE      15
#define SYS_FORK        16
#define SYS_KILL        19
#define SYS_IOCTL       21
#define SYS_MKDIR       28
#define SYS_UNLINK      29
#define SYS_RENAME      39
#define SYS_BRK         41
#define SYS_LINK        54
#define SYS_TIMES       84
#define SYS_GETTIMEOFDAY 127

/* ---- Raw syscall helpers ---- */

static inline int _sc0(int nr) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(nr) : "memory");
    return ret;
}

static inline int _sc1(int nr, int a1) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(nr), "b"(a1) : "memory");
    return ret;
}

static inline int _sc2(int nr, int a1, int a2) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(nr), "b"(a1), "c"(a2) : "memory");
    return ret;
}

static inline int _sc3(int nr, int a1, int a2, int a3) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(nr), "b"(a1), "c"(a2), "d"(a3) : "memory");
    return ret;
}

/* Convert negative syscall return to errno + return -1 */
static inline int _check(int r) {
    if (r < 0) {
        errno = -r;
        return -1;
    }
    return r;
}

/* ---- Heap management via brk() ---- */

static char *_heap_end = 0;

caddr_t sbrk(int incr) {
    if (!_heap_end) {
        /* Get current break */
        _heap_end = (char *)(uintptr_t)_sc1(SYS_BRK, 0);
        if ((intptr_t)_heap_end < 0) {
            errno = ENOMEM;
            return (caddr_t)-1;
        }
    }

    char *prev_end = _heap_end;
    char *new_end = _heap_end + incr;

    int result = _sc1(SYS_BRK, (int)new_end);
    if ((uintptr_t)result < (uintptr_t)new_end) {
        errno = ENOMEM;
        return (caddr_t)-1;
    }

    _heap_end = new_end;
    return (caddr_t)prev_end;
}

/* ---- File I/O ---- */

int open(const char *name, int flags, int mode) {
    (void)mode;
    return _check(_sc2(SYS_OPEN, (int)name, flags));
}

int close(int fd) {
    return _check(_sc1(SYS_CLOSE, fd));
}

int read(int fd, char *buf, int len) {
    return _check(_sc3(SYS_READ, fd, (int)buf, len));
}

int write(int fd, const char *buf, int len) {
    return _check(_sc3(SYS_WRITE, fd, (int)buf, len));
}

int lseek(int fd, int offset, int whence) {
    return _check(_sc3(SYS_LSEEK, fd, offset, whence));
}

int fstat(int fd, struct stat *st) {
    return _check(_sc2(SYS_FSTAT, fd, (int)st));
}

int stat(const char *path, struct stat *st) {
    return _check(_sc2(SYS_STAT, (int)path, (int)st));
}

int link(const char *oldpath, const char *newpath) {
    return _check(_sc2(SYS_LINK, (int)oldpath, (int)newpath));
}

int unlink(const char *name) {
    return _check(_sc1(SYS_UNLINK, (int)name));
}

/* rename is provided by newlib libc.a */

int mkdir(const char *path, mode_t mode) {
    (void)mode;
    return _check(_sc1(SYS_MKDIR, (int)path));
}

int isatty(int fd) {
    /* Use ioctl TCGETS (0x5401) — if it succeeds, fd is a tty.
     * TCGETS is the standard test: it succeeds on any terminal device
     * regardless of process group state. TIOCGPGRP was unreliable because
     * it requires a valid output pointer and a foreground pgrp to be set. */
    struct { uint32_t a, b, c, d; uint8_t e[8]; } t;
    int r = _sc3(SYS_IOCTL, fd, 0x5401, (int)&t);
    if (r < 0) {
        errno = ENOTTY;
        return 0;
    }
    return 1;
}

/* ---- Process control ---- */

void _exit(int status) {
    _sc1(SYS_EXIT, status);
    __builtin_unreachable();
}

int getpid(void) {
    return _sc0(SYS_GETPID);
}

int kill(int pid, int sig) {
    return _check(_sc2(SYS_KILL, pid, sig));
}

int fork(void) {
    return _check(_sc0(SYS_FORK));
}

int execve(const char *name, char *const argv[], char *const envp[]) {
    return _check(_sc3(SYS_EXECVE, (int)name, (int)argv, (int)envp));
}

int wait(int *status) {
    return _check(_sc3(SYS_WAITPID, -1, (int)status, 0));
}

/* ---- Time ---- */

int gettimeofday(struct timeval *tv, void *tz) {
    (void)tz;
    return _check(_sc2(SYS_GETTIMEOFDAY, (int)tv, 0));
}

clock_t times(struct tms *buf) {
    return (clock_t)_check(_sc1(SYS_TIMES, (int)buf));
}
