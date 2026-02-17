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

int mkdir(const char* path, ...) {
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

int pread(int fd, void* buf, size_t count, int offset) {
    return __syscall_ret(_syscall4(SYS_PREAD, fd, (int)buf, (int)count, offset));
}

int pwrite(int fd, const void* buf, size_t count, int offset) {
    return __syscall_ret(_syscall4(SYS_PWRITE, fd, (int)buf, (int)count, offset));
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

int truncate(const char* path, int length) {
    return __syscall_ret(_syscall2(SYS_TRUNCATE, (int)path, length));
}

int ftruncate(int fd, int length) {
    return __syscall_ret(_syscall2(SYS_FTRUNCATE, fd, length));
}

unsigned int alarm(unsigned int seconds) {
    return (unsigned int)_syscall1(SYS_ALARM, (int)seconds);
}

int setitimer(int which, const void* new_value, void* old_value) {
    return __syscall_ret(_syscall3(SYS_SETITIMER, which, (int)new_value, (int)old_value));
}

int getitimer(int which, void* curr_value) {
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

int stat(const char* path, void* buf) {
    return __syscall_ret(_syscall2(SYS_STAT, (int)path, (int)buf));
}

int fstat(int fd, void* buf) {
    return __syscall_ret(_syscall2(SYS_FSTAT, fd, (int)buf));
}

int chmod(const char* path, int mode) {
    return __syscall_ret(_syscall2(SYS_CHMOD, (int)path, mode));
}

int chown(const char* path, int owner, int group) {
    return __syscall_ret(_syscall3(SYS_CHOWN, (int)path, owner, group));
}

int link(const char* oldpath, const char* newpath) {
    /* Use SYS_UNLINKAT slot 38 — AdrOS doesn't have a dedicated link syscall yet,
     * use a direct int $0x80 with the link syscall number if available */
    (void)oldpath; (void)newpath;
    return -1;  /* TODO: implement when kernel has SYS_LINK */
}

int symlink(const char* target, const char* linkpath) {
    (void)target; (void)linkpath;
    return -1;  /* TODO: implement when kernel has SYS_SYMLINK */
}

int readlink(const char* path, char* buf, size_t bufsiz) {
    (void)path; (void)buf; (void)bufsiz;
    return -1;  /* TODO: implement when kernel has SYS_READLINK */
}

void _exit(int status) {
    _syscall1(SYS_EXIT, status);
    /* If exit syscall somehow returns, loop forever.
     * Cannot use hlt — it's privileged and causes #GP in ring 3. */
    for (;;) __asm__ volatile("nop");
}
