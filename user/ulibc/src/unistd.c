// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#include "unistd.h"
#include "syscall.h"
#include "errno.h"
#include "termios.h"
#include "stdlib.h"
#include "sys/select.h"
#include "sys/time.h"
#include "sys/stat.h"
#include "sys/resource.h"
#include "signal.h"

int read(int fd, void* buf, size_t count) {
    return __syscall_ret(_syscall3(SYS_READ, fd, (int)buf, (int)count));
}

int write(int fd, const void* buf, size_t count) {
    return __syscall_ret(_syscall3(SYS_WRITE, fd, (int)buf, (int)count));
}

int open(const char* path, int flags, ...) {
    __builtin_va_list ap;
    __builtin_va_start(ap, flags);
    int mode = __builtin_va_arg(ap, int);
    __builtin_va_end(ap);
    (void)mode; /* kernel ignores mode currently */
    return __syscall_ret(_syscall2(SYS_OPEN, (int)path, flags));
}

int close(int fd) {
    return __syscall_ret(_syscall1(SYS_CLOSE, fd));
}

off_t lseek(int fd, off_t offset, int whence) {
    return __syscall_ret(_syscall3(SYS_LSEEK, fd, (int)offset, whence));
}

int dup(int oldfd) {
    return __syscall_ret(_syscall1(SYS_DUP, oldfd));
}

int dup2(int oldfd, int newfd) {
    return __syscall_ret(_syscall2(SYS_DUP2, oldfd, newfd));
}

int pipe(int fds[2]) {
    return __syscall_ret(_syscall1(SYS_PIPE, (int)fds));
}

int fork(void) {
    return __syscall_ret(_syscall0(SYS_FORK));
}

int execve(const char* path, char* const argv[], char* const envp[]) {
    return __syscall_ret(_syscall3(SYS_EXECVE, (int)path, (int)argv, (int)envp));
}

int execveat(int dirfd, const char* path, char* const argv[], char* const envp[], int flags) {
    return __syscall_ret(_syscall5(SYS_EXECVEAT, dirfd, (int)path, (int)argv, (int)envp, flags));
}

int getpid(void) {
    return _syscall0(SYS_GETPID);
}

int getppid(void) {
    return _syscall0(SYS_GETPPID);
}

int chdir(const char* path) {
    return __syscall_ret(_syscall1(SYS_CHDIR, (int)path));
}

char* getcwd(char* buf, size_t size) {
    int allocated = 0;
    if (!buf) {
        if (size == 0) size = 4096;
        buf = (char*)malloc(size);
        if (!buf) { errno = ENOMEM; return NULL; }
        allocated = 1;
    }
    int r = _syscall2(SYS_GETCWD, (int)buf, (int)size);
    if (r < 0) {
        errno = -r;
        if (allocated) free(buf);
        return NULL;
    }
    return buf;
}

int mkdir(const char* path, mode_t mode) {
    (void)mode; /* kernel ignores mode currently */
    return __syscall_ret(_syscall1(SYS_MKDIR, (int)path));
}

int unlink(const char* path) {
    return __syscall_ret(_syscall1(SYS_UNLINK, (int)path));
}

int rmdir(const char* path) {
    return __syscall_ret(_syscall1(SYS_RMDIR, (int)path));
}

int setsid(void) {
    return __syscall_ret(_syscall0(SYS_SETSID));
}

int setpgid(int pid, int pgid) {
    return __syscall_ret(_syscall2(SYS_SETPGID, pid, pgid));
}

int getpgrp(void) {
    return __syscall_ret(_syscall0(SYS_GETPGRP));
}

int gettid(void) {
    return _syscall0(SYS_GETTID);
}

int fsync(int fd) {
    return __syscall_ret(_syscall1(SYS_FSYNC, fd));
}

int fdatasync(int fd) {
    return __syscall_ret(_syscall1(SYS_FDATASYNC, fd));
}

ssize_t pread(int fd, void* buf, size_t count, off_t offset) {
    return __syscall_ret(_syscall4(SYS_PREAD, fd, (int)buf, (int)count, (int)offset));
}

ssize_t pwrite(int fd, const void* buf, size_t count, off_t offset) {
    return __syscall_ret(_syscall4(SYS_PWRITE, fd, (int)buf, (int)count, (int)offset));
}

int access(const char* path, int mode) {
    return __syscall_ret(_syscall2(SYS_ACCESS, (int)path, mode));
}

int getuid(void) {
    return _syscall0(SYS_GETUID);
}

int getgid(void) {
    return _syscall0(SYS_GETGID);
}

int geteuid(void) {
    return _syscall0(SYS_GETEUID);
}

int getegid(void) {
    return _syscall0(SYS_GETEGID);
}

int setuid(int uid) {
    return __syscall_ret(_syscall1(SYS_SETUID, uid));
}

int setgid(int gid) {
    return __syscall_ret(_syscall1(SYS_SETGID, gid));
}

int seteuid(int euid) {
    return __syscall_ret(_syscall1(SYS_SETEUID, euid));
}

int setegid(int egid) {
    return __syscall_ret(_syscall1(SYS_SETEGID, egid));
}

int truncate(const char* path, off_t length) {
    return __syscall_ret(_syscall2(SYS_TRUNCATE, (int)path, (int)length));
}

int ftruncate(int fd, off_t length) {
    return __syscall_ret(_syscall2(SYS_FTRUNCATE, fd, (int)length));
}

unsigned int alarm(unsigned int seconds) {
    return (unsigned int)_syscall1(SYS_ALARM, (int)seconds);
}

int setitimer(int which, const struct itimerval* new_value, struct itimerval* old_value) {
    return __syscall_ret(_syscall3(SYS_SETITIMER, which, (int)new_value, (int)old_value));
}

int getitimer(int which, struct itimerval* curr_value) {
    return __syscall_ret(_syscall2(SYS_GETITIMER, which, (int)curr_value));
}

int flock(int fd, int operation) {
    return __syscall_ret(_syscall2(SYS_FLOCK, fd, operation));
}

void* brk(void* addr) {
    return (void*)_syscall1(SYS_BRK, (int)addr);
}

int isatty(int fd) {
    /* POSIX: isatty() returns 1 if fd refers to a terminal, 0 otherwise.
     * Implementation: try TCGETS ioctl; if it succeeds, fd is a tty. */
    struct { uint32_t a,b,c,d; uint8_t e[8]; } t;
    int rc = _syscall3(SYS_IOCTL, fd, 0x5401 /* TCGETS */, (int)&t);
    return (rc == 0) ? 1 : 0;
}

int waitpid(int pid, int* status, int options) {
    return __syscall_ret(_syscall3(SYS_WAITPID, pid, (int)status, options));
}

int getdents(int fd, void* buf, size_t count) {
    return __syscall_ret(_syscall3(SYS_GETDENTS, fd, (int)buf, (int)count));
}

int stat(const char* path, struct stat* buf) {
    return __syscall_ret(_syscall2(SYS_STAT, (int)path, (int)buf));
}

int fstat(int fd, struct stat* buf) {
    return __syscall_ret(_syscall2(SYS_FSTAT, fd, (int)buf));
}

int lstat(const char* path, struct stat* buf) {
    return stat(path, buf);  /* no symlinks in AdrOS, same as stat */
}

int chmod(const char* path, mode_t mode) {
    return __syscall_ret(_syscall2(SYS_CHMOD, (int)path, (int)mode));
}

int fchmod(int fd, mode_t mode) {
    (void)fd; (void)mode;
    errno = ENOSYS;
    return -1;
}

int chown(const char* path, int owner, int group) {
    return __syscall_ret(_syscall3(SYS_CHOWN, (int)path, owner, group));
}

int link(const char* oldpath, const char* newpath) {
    return __syscall_ret(_syscall2(SYS_LINK, (int)oldpath, (int)newpath));
}

int symlink(const char* target, const char* linkpath) {
    return __syscall_ret(_syscall2(SYS_SYMLINK, (int)target, (int)linkpath));
}

int readlink(const char* path, char* buf, size_t bufsiz) {
    return __syscall_ret(_syscall3(SYS_READLINK, (int)path, (int)buf, (int)bufsiz));
}

int tcgetattr(int fd, struct termios* t) {
    return __syscall_ret(_syscall3(SYS_IOCTL, fd, 0x5401 /* TCGETS */, (int)t));
}

int tcsetattr(int fd, int actions, const struct termios* t) {
    int cmd = 0x5402; /* TCSETS */
    if (actions == 1) cmd = 0x5403; /* TCSETSW */
    else if (actions == 2) cmd = 0x5404; /* TCSETSF */
    return __syscall_ret(_syscall3(SYS_IOCTL, fd, cmd, (int)t));
}

void _exit(int status) {
    _syscall1(SYS_EXIT, status);
    /* If exit syscall somehow returns, loop forever.
     * Cannot use hlt — it's privileged and causes #GP in ring 3. */
    for (;;) __asm__ volatile("nop");
}

int execle(const char* path, const char* arg, ...) {
    /* Walk varargs to find argv[] and the trailing envp */
    const char* args[32];
    int n = 0;
    __builtin_va_list ap;
    __builtin_va_start(ap, arg);
    args[n++] = arg;
    while (n < 31) {
        const char* a = __builtin_va_arg(ap, const char*);
        args[n++] = a;
        if (!a) break;
    }
    args[n] = (void*)0;
    char* const* envp = __builtin_va_arg(ap, char* const*);
    __builtin_va_end(ap);
    return execve(path, (char* const*)args, envp);
}

static char _login_buf[32] = "root";
char* getlogin(void) {
    return _login_buf;
}

int getlogin_r(char* buf, size_t bufsize) {
    extern size_t strlen(const char*);
    extern void*  memcpy(void*, const void*, size_t);
    const char* name = getlogin();
    size_t len = strlen(name);
    if (len + 1 > bufsize) return 34; /* ERANGE */
    memcpy(buf, name, len + 1);
    return 0;
}

long confstr(int name, char* buf, size_t len) {
    (void)name;
    const char* val = "/bin:/usr/bin";
    extern size_t strlen(const char*);
    size_t vlen = strlen(val) + 1;
    if (buf && len > 0) {
        size_t copy = vlen < len ? vlen : len;
        extern void* memcpy(void*, const void*, size_t);
        memcpy(buf, val, copy);
        if (copy < vlen && len > 0) buf[len - 1] = '\0';
    }
    return (long)vlen;
}

void* sbrk(int increment) {
    void* cur = brk((void*)0);
    if (increment == 0) return cur;
    void* new_end = (void*)((char*)cur + increment);
    void* result = brk(new_end);
    if ((unsigned int)result < (unsigned int)new_end) return (void*)-1;
    return cur;
}

int select(int nfds, fd_set* readfds, fd_set* writefds, fd_set* exceptfds,
           struct timeval* timeout) {
    /* Kernel expects int32_t timeout: -1 = infinite, 0 = poll, >0 = ticks.
     * TIMER_HZ = 100, so 1 tick = 10 ms. */
    int32_t tmo = -1;
    if (timeout) {
        if (timeout->tv_sec == 0 && timeout->tv_usec == 0) {
            tmo = 0;
        } else {
            uint32_t ms = (uint32_t)timeout->tv_sec * 1000
                        + (uint32_t)timeout->tv_usec / 1000;
            tmo = (int32_t)(ms / 10);
            if (tmo < 1) tmo = 1;
        }
    }
    return __syscall_ret(_syscall5(SYS_SELECT, nfds, (int)readfds, (int)writefds,
                                   (int)exceptfds, (int)tmo));
}

int fcntl(int fd, int cmd, ...) {
    __builtin_va_list ap;
    __builtin_va_start(ap, cmd);
    int arg = __builtin_va_arg(ap, int);
    __builtin_va_end(ap);
    return __syscall_ret(_syscall3(SYS_FCNTL, fd, cmd, arg));
}

int rename(const char* oldpath, const char* newpath) {
    return __syscall_ret(_syscall2(SYS_RENAME, (int)oldpath, (int)newpath));
}

int umask(int mask) {
    return _syscall1(SYS_UMASK, mask);
}

int dup3(int oldfd, int newfd, int flags) {
    return __syscall_ret(_syscall3(SYS_DUP3, oldfd, newfd, flags));
}

int openat(int dirfd, const char* path, int flags, ...) {
    __builtin_va_list ap;
    __builtin_va_start(ap, flags);
    int mode = __builtin_va_arg(ap, int);
    __builtin_va_end(ap);
    return __syscall_ret(_syscall4(SYS_OPENAT, dirfd, (int)path, flags, mode));
}

int fstatat(int dirfd, const char* path, struct stat* buf, int flags) {
    return __syscall_ret(_syscall4(SYS_FSTATAT, dirfd, (int)path, (int)buf, flags));
}

int unlinkat(int dirfd, const char* path, int flags) {
    return __syscall_ret(_syscall3(SYS_UNLINKAT, dirfd, (int)path, flags));
}

int mount(const char* source, const char* target, const char* fs_type, unsigned long flags, const void* data) {
    return __syscall_ret(_syscall5(SYS_MOUNT, (int)source, (int)target, (int)fs_type, (int)flags, (int)data));
}

int umount2(const char* target, int flags) {
    return __syscall_ret(_syscall2(SYS_UMOUNT2, (int)target, flags));
}

int umount(const char* target) {
    return umount2(target, 0);
}

pid_t wait4(pid_t pid, int* status, int options, struct rusage* rusage) {
    return __syscall_ret(_syscall4(SYS_WAIT4, (int)pid, (int)status, options, (int)rusage));
}

int waitid(int idtype, int id, siginfo_t* info, int options) {
    return __syscall_ret(_syscall4(SYS_WAITID, idtype, id, (int)info, options));
}

int sigreturn(void) {
    return _syscall0(SYS_SIGRETURN);
}

int sigqueue(int pid, int sig, const union sigval value) {
    return __syscall_ret(_syscall3(SYS_SIGQUEUE, pid, sig, (int)value.sival_int));
}

int set_thread_area(void* desc) {
    return __syscall_ret(_syscall1(SYS_SET_THREAD_AREA, (int)desc));
}

int pivot_root(const char* new_root, const char* put_old) {
    return __syscall_ret(_syscall2(SYS_PIVOT_ROOT, (int)new_root, (int)put_old));
}
