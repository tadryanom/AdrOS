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
#include <errno.h>
#include <stdint.h>

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

/* ---- Environment ---- */
char *__env[1] = { 0 };
char **environ = __env;

/* ---- Heap management via brk() ---- */

static char *_heap_end = 0;

caddr_t _sbrk(int incr) {
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

int _open(const char *name, int flags, int mode) {
    (void)mode;
    return _check(_sc2(SYS_OPEN, (int)name, flags));
}

int _close(int fd) {
    return _check(_sc1(SYS_CLOSE, fd));
}

int _read(int fd, char *buf, int len) {
    return _check(_sc3(SYS_READ, fd, (int)buf, len));
}

int _write(int fd, const char *buf, int len) {
    return _check(_sc3(SYS_WRITE, fd, (int)buf, len));
}

int _lseek(int fd, int offset, int whence) {
    return _check(_sc3(SYS_LSEEK, fd, offset, whence));
}

int _fstat(int fd, struct stat *st) {
    return _check(_sc2(SYS_FSTAT, fd, (int)st));
}

int _stat(const char *path, struct stat *st) {
    return _check(_sc2(SYS_STAT, (int)path, (int)st));
}

int _link(const char *oldpath, const char *newpath) {
    return _check(_sc2(SYS_LINK, (int)oldpath, (int)newpath));
}

int _unlink(const char *name) {
    return _check(_sc1(SYS_UNLINK, (int)name));
}

int _rename(const char *oldpath, const char *newpath) {
    return _check(_sc2(SYS_RENAME, (int)oldpath, (int)newpath));
}

int _mkdir(const char *path, int mode) {
    (void)mode;
    return _check(_sc1(SYS_MKDIR, (int)path));
}

int _isatty(int fd) {
    /* Use ioctl TIOCGPGRP (0x540F) — if it succeeds, fd is a tty */
    int r = _sc3(SYS_IOCTL, fd, 0x540F, 0);
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

int _getpid(void) {
    return _sc0(SYS_GETPID);
}

int _kill(int pid, int sig) {
    return _check(_sc2(SYS_KILL, pid, sig));
}

int _fork(void) {
    return _check(_sc0(SYS_FORK));
}

int _execve(const char *name, char *const argv[], char *const envp[]) {
    return _check(_sc3(SYS_EXECVE, (int)name, (int)argv, (int)envp));
}

int _wait(int *status) {
    return _check(_sc3(SYS_WAITPID, -1, (int)status, 0));
}

/* ---- Time ---- */

int _gettimeofday(struct timeval *tv, void *tz) {
    (void)tz;
    return _check(_sc2(SYS_GETTIMEOFDAY, (int)tv, 0));
}

int _times(struct tms *buf) {
    return _check(_sc1(SYS_TIMES, (int)buf));
}
