// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef ULIBC_SYSCALL_H
#define ULIBC_SYSCALL_H

#include <stdint.h>

enum {
    SYS_WRITE = 1,
    SYS_EXIT  = 2,
    SYS_GETPID = 3,
    SYS_OPEN  = 4,
    SYS_READ  = 5,
    SYS_CLOSE = 6,
    SYS_WAITPID = 7,
    SYS_LSEEK = 9,
    SYS_FSTAT = 10,
    SYS_STAT = 11,
    SYS_DUP = 12,
    SYS_DUP2 = 13,
    SYS_PIPE = 14,
    SYS_EXECVE = 15,
    SYS_FORK = 16,
    SYS_GETPPID = 17,
    SYS_POLL = 18,
    SYS_KILL = 19,
    SYS_SELECT = 20,
    SYS_IOCTL = 21,
    SYS_SETSID = 22,
    SYS_SETPGID = 23,
    SYS_GETPGRP = 24,
    SYS_SIGACTION = 25,
    SYS_SIGPROCMASK = 26,
    SYS_SIGRETURN = 27,
    SYS_MKDIR = 28,
    SYS_UNLINK = 29,
    SYS_GETDENTS = 30,
    SYS_FCNTL = 31,
    SYS_CHDIR = 32,
    SYS_GETCWD = 33,
    SYS_PIPE2 = 34,
    SYS_DUP3 = 35,
    SYS_OPENAT = 36,
    SYS_FSTATAT = 37,
    SYS_UNLINKAT = 38,
    SYS_RENAME = 39,
    SYS_RMDIR = 40,
    SYS_BRK = 41,
    SYS_NANOSLEEP = 42,
    SYS_CLOCK_GETTIME = 43,
    SYS_MMAP = 44,
    SYS_MUNMAP = 45,
    SYS_SET_THREAD_AREA = 57,
    SYS_CLONE = 67,
    SYS_GETTID = 68,
    SYS_FSYNC = 69,
    SYS_FDATASYNC = 70,
    SYS_SIGPENDING = 71,
    SYS_PREAD = 72,
    SYS_PWRITE = 73,
    SYS_ACCESS = 74,
    SYS_UMASK = 75,
    SYS_CHMOD = 50,
    SYS_CHOWN = 51,
    SYS_GETUID = 52,
    SYS_GETGID = 53,
    SYS_SETUID = 76,
    SYS_SETGID = 77,
    SYS_TRUNCATE = 78,
    SYS_FTRUNCATE = 79,
    SYS_SIGSUSPEND = 80,
    SYS_READV = 81,
    SYS_WRITEV = 82,
    SYS_ALARM = 83,
    SYS_TIMES = 84,
    SYS_FUTEX = 85,
    SYS_SIGALTSTACK = 86,
    SYS_FLOCK = 87,
    SYS_GETEUID = 88,
    SYS_GETEGID = 89,
    SYS_SETEUID = 90,
    SYS_SETEGID = 91,
    SYS_SETITIMER = 92,
    SYS_GETITIMER = 93,
    SYS_SHMGET = 46,
    SYS_SHMAT  = 47,
    SYS_SHMDT  = 48,
    SYS_SHMCTL = 49,
    SYS_LINK   = 54,
    SYS_SYMLINK = 55,
    SYS_READLINK = 56,
    SYS_SOCKET    = 58,
    SYS_BIND      = 59,
    SYS_LISTEN    = 60,
    SYS_ACCEPT    = 61,
    SYS_CONNECT   = 62,
    SYS_SEND      = 63,
    SYS_RECV      = 64,
    SYS_SENDTO    = 65,
    SYS_RECVFROM  = 66,
    SYS_WAITID    = 94,
    SYS_SIGQUEUE  = 95,
    SYS_POSIX_SPAWN = 96,
    SYS_MQ_OPEN     = 97,
    SYS_MQ_CLOSE    = 98,
    SYS_MQ_SEND     = 99,
    SYS_MQ_RECEIVE  = 100,
    SYS_MQ_UNLINK   = 101,
    SYS_SEM_OPEN    = 102,
    SYS_SEM_CLOSE   = 103,
    SYS_SEM_WAIT    = 104,
    SYS_SEM_POST    = 105,
    SYS_SEM_UNLINK  = 106,
    SYS_SEM_GETVALUE = 107,
    SYS_GETADDRINFO  = 108,
    SYS_DLOPEN       = 109,
    SYS_DLSYM        = 110,
    SYS_DLCLOSE      = 111,
    SYS_EPOLL_CREATE = 112,
    SYS_EPOLL_CTL    = 113,
    SYS_EPOLL_WAIT   = 114,
    SYS_INOTIFY_INIT      = 115,
    SYS_INOTIFY_ADD_WATCH = 116,
    SYS_INOTIFY_RM_WATCH  = 117,
    SYS_SENDMSG  = 118,
    SYS_RECVMSG  = 119,
    SYS_PIVOT_ROOT = 120,
    SYS_AIO_READ    = 121,
    SYS_AIO_WRITE   = 122,
    SYS_AIO_ERROR   = 123,
    SYS_AIO_RETURN  = 124,
    SYS_AIO_SUSPEND = 125,
    SYS_MOUNT = 126,
    SYS_GETTIMEOFDAY = 127,
    SYS_MPROTECT = 128,
    SYS_GETRLIMIT = 129,
    SYS_SETRLIMIT = 130,
    SYS_SETSOCKOPT = 131,
    SYS_GETSOCKOPT = 132,
    SYS_SHUTDOWN = 133,
    SYS_GETPEERNAME = 134,
    SYS_GETSOCKNAME = 135,
    SYS_UNAME = 136,
    SYS_GETRUSAGE = 137,
    SYS_UMOUNT2 = 138,
    SYS_WAIT4 = 139,
    SYS_MADVISE = 140,
    SYS_EXECVEAT = 141,
};

/* Raw syscall wrappers — up to 5 args via INT 0x80 */
static inline int _syscall0(int nr) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(nr) : "memory");
    return ret;
}

static inline int _syscall1(int nr, int a1) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(nr), "b"(a1) : "memory");
    return ret;
}

static inline int _syscall2(int nr, int a1, int a2) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(nr), "b"(a1), "c"(a2) : "memory");
    return ret;
}

static inline int _syscall3(int nr, int a1, int a2, int a3) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(nr), "b"(a1), "c"(a2), "d"(a3) : "memory");
    return ret;
}

static inline int _syscall4(int nr, int a1, int a2, int a3, int a4) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(nr), "b"(a1), "c"(a2), "d"(a3), "S"(a4) : "memory");
    return ret;
}

static inline int _syscall5(int nr, int a1, int a2, int a3, int a4, int a5) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(nr), "b"(a1), "c"(a2), "d"(a3), "S"(a4), "D"(a5) : "memory");
    return ret;
}

#endif
