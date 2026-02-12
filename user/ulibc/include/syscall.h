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
