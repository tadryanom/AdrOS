#include "unistd.h"
#include "syscall.h"
#include "errno.h"

int read(int fd, void* buf, size_t count) {
    return __syscall_ret(_syscall3(SYS_READ, fd, (int)buf, (int)count));
}

int write(int fd, const void* buf, size_t count) {
    return __syscall_ret(_syscall3(SYS_WRITE, fd, (int)buf, (int)count));
}

int open(const char* path, int flags) {
    return __syscall_ret(_syscall2(SYS_OPEN, (int)path, flags));
}

int close(int fd) {
    return __syscall_ret(_syscall1(SYS_CLOSE, fd));
}

int lseek(int fd, int offset, int whence) {
    return __syscall_ret(_syscall3(SYS_LSEEK, fd, offset, whence));
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

int execve(const char* path, const char* const* argv, const char* const* envp) {
    return __syscall_ret(_syscall3(SYS_EXECVE, (int)path, (int)argv, (int)envp));
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

int getcwd(char* buf, size_t size) {
    return __syscall_ret(_syscall2(SYS_GETCWD, (int)buf, (int)size));
}

int mkdir(const char* path) {
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

void* brk(void* addr) {
    return (void*)_syscall1(SYS_BRK, (int)addr);
}

void _exit(int status) {
    _syscall1(SYS_EXIT, status);
    /* If exit syscall somehow returns, loop forever.
     * Cannot use hlt — it's privileged and causes #GP in ring 3. */
    for (;;) __asm__ volatile("nop");
}
