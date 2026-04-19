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
 * posix_stubs.c — POSIX syscall wrappers for AdrOS/newlib (i686)
 *
 * These functions provide the POSIX API layer on top of AdrOS kernel syscalls.
 * Each function issues an AdrOS syscall via INT 0x80.
 *
 * AdrOS syscall convention (i386):
 *   EAX = syscall number
 *   EBX = arg1, ECX = arg2, EDX = arg3, ESI = arg4, EDI = arg5
 *   Return value in EAX (negative = -errno)
 */

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/utsname.h>
#include <sys/uio.h>
#include <sys/times.h>
#include <sys/wait.h>
#include <termios.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <pwd.h>
#include <sys/ioctl.h>
#include <dirent.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <fcntl.h>
#include <regex.h>
#include <fnmatch.h>
#include <time.h>

/* ---- AdrOS syscall numbers (must match include/syscall.h) ---- */
#define SYS_EXIT        2
#define SYS_GETPID      3
#define SYS_OPEN        4
#define SYS_READ        5
#define SYS_CLOSE       6
#define SYS_WAITPID     7
#define SYS_LSEEK       9
#define SYS_FSTAT       10
#define SYS_STAT        11
#define SYS_DUP         12
#define SYS_DUP2        13
#define SYS_PIPE        14
#define SYS_EXECVE      15
#define SYS_FORK        16
#define SYS_GETPPID     17
#define SYS_POLL        18
#define SYS_KILL        19
#define SYS_SELECT      20
#define SYS_IOCTL       21
#define SYS_SETSID      22
#define SYS_SETPGID     23
#define SYS_GETPGRP     24
#define SYS_SIGACTION   25
#define SYS_SIGPROCMASK 26
#define SYS_SIGRETURN   27
#define SYS_MKDIR       28
#define SYS_UNLINK      29
#define SYS_GETDENTS    30
#define SYS_FCNTL       31
#define SYS_CHDIR       32
#define SYS_GETCWD      33
#define SYS_PIPE2       34
#define SYS_DUP3        35
#define SYS_OPENAT      36
#define SYS_FSTATAT     37
#define SYS_UNLINKAT    38
#define SYS_RENAME      39
#define SYS_RMDIR       40
#define SYS_BRK         41
#define SYS_NANOSLEEP   42
#define SYS_CLOCK_GETTIME 43
#define SYS_MMAP        44
#define SYS_MUNMAP      45
#define SYS_SHMGET      46
#define SYS_SHMAT       47
#define SYS_SHMDT       48
#define SYS_SHMCTL      49
#define SYS_CHMOD       50
#define SYS_CHOWN       51
#define SYS_GETUID      52
#define SYS_GETGID      53
#define SYS_LINK        54
#define SYS_SYMLINK     55
#define SYS_READLINK    56
#define SYS_SET_THREAD_AREA 57
#define SYS_SOCKET      58
#define SYS_BIND        59
#define SYS_LISTEN      60
#define SYS_ACCEPT      61
#define SYS_CONNECT     62
#define SYS_SEND        63
#define SYS_RECV        64
#define SYS_SENDTO      65
#define SYS_RECVFROM    66
#define SYS_CLONE       67
#define SYS_GETTID      68
#define SYS_FSYNC       69
#define SYS_FDATASYNC   70
#define SYS_SIGPENDING  71
#define SYS_PREAD       72
#define SYS_PWRITE      73
#define SYS_ACCESS      74
#define SYS_UMASK       75
#define SYS_SETUID      76
#define SYS_SETGID      77
#define SYS_TRUNCATE    78
#define SYS_FTRUNCATE   79
#define SYS_SIGSUSPEND  80
#define SYS_READV       81
#define SYS_WRITEV      82
#define SYS_ALARM       83
#define SYS_TIMES       84
#define SYS_FUTEX       85
#define SYS_SIGALTSTACK 86
#define SYS_FLOCK       87
#define SYS_GETEUID     88
#define SYS_GETEGID     89
#define SYS_SETEUID     90
#define SYS_SETEGID     91
#define SYS_SETITIMER   92
#define SYS_GETITIMER   93
#define SYS_WAITID      94
#define SYS_SIGQUEUE    95
#define SYS_POSIX_SPAWN 96
#define SYS_MQ_OPEN     97
#define SYS_MQ_CLOSE    98
#define SYS_MQ_SEND     99
#define SYS_MQ_RECEIVE  100
#define SYS_MQ_UNLINK   101
#define SYS_SEM_OPEN    102
#define SYS_SEM_CLOSE   103
#define SYS_SEM_WAIT    104
#define SYS_SEM_POST    105
#define SYS_SEM_UNLINK  106
#define SYS_SEM_GETVALUE 107
#define SYS_GETADDRINFO 108
#define SYS_DLOPEN      109
#define SYS_DLSYM       110
#define SYS_DLCLOSE     111
#define SYS_EPOLL_CREATE 112
#define SYS_EPOLL_CTL   113
#define SYS_EPOLL_WAIT  114
#define SYS_INOTIFY_INIT     115
#define SYS_INOTIFY_ADD_WATCH 116
#define SYS_INOTIFY_RM_WATCH  117
#define SYS_SENDMSG     118
#define SYS_RECVMSG     119
#define SYS_PIVOT_ROOT  120
#define SYS_AIO_READ    121
#define SYS_AIO_WRITE   122
#define SYS_AIO_ERROR   123
#define SYS_AIO_RETURN  124
#define SYS_AIO_SUSPEND 125
#define SYS_MOUNT       126
#define SYS_GETTIMEOFDAY 127
#define SYS_MPROTECT    128
#define SYS_GETRLIMIT   129
#define SYS_SETRLIMIT   130
#define SYS_SETSOCKOPT  131
#define SYS_GETSOCKOPT  132
#define SYS_SHUTDOWN    133
#define SYS_GETPEERNAME 134
#define SYS_GETSOCKNAME 135
#define SYS_UNAME       136
#define SYS_GETRUSAGE   137
#define SYS_UMOUNT2     138
#define SYS_WAIT4       139
#define SYS_MADVISE     140
#define SYS_EXECVEAT    141

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

static inline int _sc4(int nr, int a1, int a2, int a3, int a4) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret)
        : "a"(nr), "b"(a1), "c"(a2), "d"(a3), "S"(a4) : "memory");
    return ret;
}

static inline int _sc5(int nr, int a1, int a2, int a3, int a4, int a5) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret)
        : "a"(nr), "b"(a1), "c"(a2), "d"(a3), "S"(a4), "D"(a5) : "memory");
    return ret;
}

/* Convert negative syscall return to errno + return -1 */
static inline int _check(int r) {
    if (r < 0) { errno = -r; return -1; }
    return r;
}

/* ---- TTY ioctl numbers (must match kernel tty.c) ---- */
#define TTY_TCGETS    0x5401
#define TTY_TCSETS    0x5402
#define TTY_TCSETSW   0x5403
#define TTY_TCSETSF   0x5404
#define TTY_TIOCGPGRP 0x540F
#define TTY_TIOCSPGRP 0x5410
#define TTY_TIOCGWINSZ 0x5413

/* ================================================================
 * File descriptor operations
 * ================================================================ */

int dup(int fd) {
    return _check(_sc1(SYS_DUP, fd));
}

int dup2(int oldfd, int newfd) {
    return _check(_sc2(SYS_DUP2, oldfd, newfd));
}

int fcntl(int fd, int cmd, ...) {
    va_list ap;
    va_start(ap, cmd);
    int arg = va_arg(ap, int);
    va_end(ap);
    return _check(_sc3(SYS_FCNTL, fd, cmd, arg));
}

int pipe(int fds[2]) {
    return _check(_sc1(SYS_PIPE, (int)fds));
}

/* ================================================================
 * File operations
 * ================================================================ */

int chown(const char *path, uid_t owner, gid_t group) {
    return _check(_sc3(SYS_CHOWN, (int)path, (int)owner, (int)group));
}

int chmod(const char *path, mode_t mode) {
    return _check(_sc2(SYS_CHMOD, (int)path, (int)mode));
}

int lstat(const char *path, struct stat *st) {
    /* AdrOS has no separate lstat syscall yet — fall back to stat */
    return stat(path, st);
}

int access(const char *path, int mode) {
    return _check(_sc2(SYS_ACCESS, (int)path, mode));
}

int chdir(const char *path) {
    return _check(_sc1(SYS_CHDIR, (int)path));
}

char *getcwd(char *buf, size_t size) {
    int allocated = 0;
    if (!buf) {
        if (size == 0) size = 4096;
        buf = (char *)malloc(size);
        if (!buf) { errno = ENOMEM; return 0; }
        allocated = 1;
    }
    int r = _sc2(SYS_GETCWD, (int)buf, (int)size);
    if (r < 0) {
        errno = -r;
        if (allocated) free(buf);
        return 0;
    }
    return buf;
}

int fchmod(int fd, mode_t mode) {
    /* No fchmod syscall yet — return ENOSYS */
    (void)fd; (void)mode;
    errno = ENOSYS;
    return -1;
}

/* ================================================================
 * Process/signal operations
 * ================================================================ */

pid_t waitpid(pid_t pid, int *status, int options) {
    return (pid_t)_check(_sc3(SYS_WAITPID, (int)pid, (int)status, options));
}

int setpgid(pid_t pid, pid_t pgid) {
    return _check(_sc2(SYS_SETPGID, (int)pid, (int)pgid));
}

pid_t setsid(void) {
    return (pid_t)_check(_sc0(SYS_SETSID));
}

pid_t getpgrp(void) {
    return (pid_t)_sc0(SYS_GETPGRP);
}

int killpg(int pgrp, int sig) {
    return kill(-pgrp, sig);
}

pid_t getppid(void) {
    return (pid_t)_sc0(SYS_GETPPID);
}

uid_t getuid(void) { return (uid_t)_sc0(SYS_GETUID); }
uid_t geteuid(void) { return (uid_t)_sc0(SYS_GETEUID); }
gid_t getgid(void) { return (gid_t)_sc0(SYS_GETGID); }
gid_t getegid(void) { return (gid_t)_sc0(SYS_GETEGID); }

int setuid(uid_t uid) { return _check(_sc1(SYS_SETUID, (int)uid)); }
int setgid(gid_t gid) { return _check(_sc1(SYS_SETGID, (int)gid)); }

int setreuid(uid_t ruid, uid_t euid) {
    /* AdrOS has setuid/seteuid but no setreuid — approximate */
    if (ruid != (uid_t)-1) { int r = _check(_sc1(SYS_SETUID, (int)ruid)); if (r < 0) return r; }
    if (euid != (uid_t)-1) { int r = _check(_sc1(SYS_SETEUID, (int)euid)); if (r < 0) return r; }
    return 0;
}

int setregid(gid_t rgid, gid_t egid) {
    if (rgid != (gid_t)-1) { int r = _check(_sc1(SYS_SETGID, (int)rgid)); if (r < 0) return r; }
    if (egid != (gid_t)-1) { int r = _check(_sc1(SYS_SETEGID, (int)egid)); if (r < 0) return r; }
    return 0;
}

mode_t umask(mode_t mask) {
    return (mode_t)_sc1(SYS_UMASK, (int)mask);
}

unsigned alarm(unsigned seconds) {
    return (unsigned)_sc1(SYS_ALARM, (int)seconds);
}

unsigned sleep(unsigned seconds) {
    struct timespec ts = { .tv_sec = seconds, .tv_nsec = 0 };
    struct timespec rem = { 0, 0 };
    int r = _sc2(SYS_NANOSLEEP, (int)&ts, (int)&rem);
    if (r < 0) return (unsigned)rem.tv_sec;
    return 0;
}

/* ================================================================
 * Signal operations
 * ================================================================ */

int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact) {
    return _check(_sc3(SYS_SIGACTION, signum, (int)act, (int)oldact));
}

int sigprocmask(int how, const sigset_t *set, sigset_t *oldset) {
    return _check(_sc3(SYS_SIGPROCMASK, how, (int)set, (int)oldset));
}

/* ================================================================
 * Terminal operations
 * ================================================================ */

int tcgetattr(int fd, struct termios *t) {
    return _check(_sc3(SYS_IOCTL, fd, TTY_TCGETS, (int)t));
}

int tcsetattr(int fd, int optional_actions, const struct termios *t) {
    int cmd;
    switch (optional_actions) {
    case TCSADRAIN: cmd = TTY_TCSETSW; break;
    case TCSAFLUSH: cmd = TTY_TCSETSF; break;
    default:        cmd = TTY_TCSETS;   break;
    }
    return _check(_sc3(SYS_IOCTL, fd, cmd, (int)t));
}

pid_t tcgetpgrp(int fd) {
    pid_t pgrp = 0;
    int r = _sc3(SYS_IOCTL, fd, TTY_TIOCGPGRP, (int)&pgrp);
    if (r < 0) { errno = -r; return (pid_t)-1; }
    return pgrp;
}

int tcsetpgrp(int fd, pid_t pgrp) {
    return _check(_sc3(SYS_IOCTL, fd, TTY_TIOCSPGRP, (int)&pgrp));
}

int tcdrain(int fd) { (void)fd; return 0; }
int tcflow(int fd, int action) { (void)fd; (void)action; return 0; }
int tcflush(int fd, int queue_selector) { (void)fd; (void)queue_selector; return 0; }
int tcsendbreak(int fd, int duration) { (void)fd; (void)duration; return 0; }

speed_t cfgetispeed(const struct termios *t) { (void)t; return B9600; }
speed_t cfgetospeed(const struct termios *t) { (void)t; return B9600; }
int cfsetispeed(struct termios *t, speed_t speed) { (void)t; (void)speed; return 0; }
int cfsetospeed(struct termios *t, speed_t speed) { (void)t; (void)speed; return 0; }

char *ttyname(int fd) {
    /* Minimal: check if fd is a tty, return generic name */
    int r = _sc3(SYS_IOCTL, fd, TTY_TIOCGPGRP, 0);
    if (r < 0) { errno = ENOTTY; return 0; }
    return "/dev/tty";
}

int ioctl(int fd, unsigned long request, ...) {
    va_list ap;
    va_start(ap, request);
    void *arg = va_arg(ap, void *);
    va_end(ap);
    return _check(_sc3(SYS_IOCTL, fd, (int)request, (int)arg));
}

/* ================================================================
 * I/O multiplexing
 * ================================================================ */

int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
           struct timeval *timeout) {
    return _check(_sc5(SYS_SELECT, nfds, (int)readfds, (int)writefds,
                       (int)exceptfds, (int)timeout));
}

int pselect(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
            const struct timespec *timeout, const sigset_t *sigmask) {
    /* Approximate pselect with select (ignore sigmask) */
    struct timeval tv, *tvp = 0;
    (void)sigmask;
    if (timeout) {
        tv.tv_sec  = timeout->tv_sec;
        tv.tv_usec = timeout->tv_nsec / 1000;
        tvp = &tv;
    }
    return select(nfds, readfds, writefds, exceptfds, tvp);
}

/* ================================================================
 * Directory operations (wrap getdents syscall)
 * ================================================================ */

/* AdrOS getdents returns fixed-size entries: { uint32_t ino; char name[256]; } */
#define ADROS_DIRENT_SIZE 260

DIR *opendir(const char *name) {
    int fd = open(name, 0 /* O_RDONLY */, 0);
    if (fd < 0) return 0;
    DIR *d = (DIR *)malloc(sizeof(DIR));
    if (!d) { close(fd); errno = ENOMEM; return 0; }
    d->dd_fd = fd;
    d->dd_loc = 0;
    d->dd_size = 0;
    d->dd_buf = (char *)malloc(4096);
    if (!d->dd_buf) { close(fd); free(d); errno = ENOMEM; return 0; }
    return d;
}

static struct dirent _de_ret;

struct dirent *readdir(DIR *d) {
    if (!d) return 0;
    /* Refill buffer if exhausted */
    if (d->dd_loc >= d->dd_size) {
        int n = _check(_sc3(SYS_GETDENTS, d->dd_fd, (int)d->dd_buf, 4096));
        if (n <= 0) return 0;
        d->dd_size = n;
        d->dd_loc = 0;
    }
    if (d->dd_loc + ADROS_DIRENT_SIZE > d->dd_size) return 0;
    char *ent = d->dd_buf + d->dd_loc;
    uint32_t ino;
    memcpy(&ino, ent, 4);
    _de_ret.d_ino = (ino_t)ino;
    strncpy(_de_ret.d_name, ent + 4, MAXNAMLEN);
    _de_ret.d_name[MAXNAMLEN] = '\0';
    d->dd_loc += ADROS_DIRENT_SIZE;
    return &_de_ret;
}

int closedir(DIR *d) {
    if (!d) return -1;
    int r = close(d->dd_fd);
    free(d->dd_buf);
    free(d);
    return r;
}

void rewinddir(DIR *d) {
    if (!d) return;
    lseek(d->dd_fd, 0, 0);
    d->dd_loc = 0;
    d->dd_size = 0;
}

/* ================================================================
 * Misc
 * ================================================================ */

int gethostname(char *name, size_t len) {
    if (name && len > 5) { strcpy(name, "adros"); return 0; }
    errno = ENAMETOOLONG; return -1;
}

/* ================================================================
 * Additional POSIX wrappers (previously missing)
 * ================================================================ */

int fcntl(int fd, int cmd, ...) {
    va_list ap;
    va_start(ap, cmd);
    int arg = va_arg(ap, int);
    va_end(ap);
    return _check(_sc3(SYS_FCNTL, fd, cmd, arg));
}

int rename(const char *oldpath, const char *newpath) {
    return _check(_sc2(SYS_RENAME, (int)oldpath, (int)newpath));
}

int rmdir(const char *path) {
    return _check(_sc1(SYS_RMDIR, (int)path));
}

int mkdir(const char *path, mode_t mode) {
    (void)mode;
    return _check(_sc1(SYS_MKDIR, (int)path));
}

int unlink(const char *path) {
    return _check(_sc1(SYS_UNLINK, (int)path));
}

int dup3(int oldfd, int newfd, int flags) {
    return _check(_sc3(SYS_DUP3, oldfd, newfd, flags));
}

int openat(int dirfd, const char *path, int flags, ...) {
    return _check(_sc3(SYS_OPENAT, dirfd, (int)path, flags));
}

int fstatat(int dirfd, const char *path, struct stat *buf, int flags) {
    return _check(_sc4(SYS_FSTATAT, dirfd, (int)path, (int)buf, flags));
}

int unlinkat(int dirfd, const char *path, int flags) {
    return _check(_sc3(SYS_UNLINKAT, dirfd, (int)path, flags));
}

int mount(const char *source, const char *target, const char *fs_type,
          unsigned long flags, const void *data) {
    return _check(_sc5(SYS_MOUNT, (int)source, (int)target, (int)fs_type, (int)flags, (int)data));
}

int umount2(const char *target, int flags) {
    return _check(_sc2(SYS_UMOUNT2, (int)target, flags));
}

int umount(const char *target) {
    return umount2(target, 0);
}

pid_t wait4(pid_t pid, int *status, int options, struct rusage *rusage) {
    return (pid_t)_check(_sc4(SYS_WAIT4, (int)pid, (int)status, options, (int)rusage));
}

int waitid(int idtype, int id, void *info, int options) {
    return _check(_sc4(SYS_WAITID, idtype, id, (int)info, options));
}

int sigreturn(void) {
    return _sc0(SYS_SIGRETURN);
}

int sigqueue(pid_t pid, int sig, const union sigval value) {
    return _check(_sc3(SYS_SIGQUEUE, (int)pid, sig, (int)value.sival_int));
}

int set_thread_area(void *desc) {
    return _check(_sc1(SYS_SET_THREAD_AREA, (int)desc));
}

ssize_t pread(int fd, void *buf, size_t count, off_t offset) {
    return (ssize_t)_check(_sc4(SYS_PREAD, fd, (int)buf, (int)count, (int)offset));
}

ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset) {
    return (ssize_t)_check(_sc4(SYS_PWRITE, fd, (int)buf, (int)count, (int)offset));
}

int truncate(const char *path, off_t length) {
    return _check(_sc2(SYS_TRUNCATE, (int)path, (int)length));
}

int ftruncate(int fd, off_t length) {
    return _check(_sc2(SYS_FTRUNCATE, fd, (int)length));
}

int fsync(int fd) {
    return _check(_sc1(SYS_FSYNC, fd));
}

int fdatasync(int fd) {
    return _check(_sc1(SYS_FDATASYNC, fd));
}

int sigpending(sigset_t *set) {
    return _check(_sc1(SYS_SIGPENDING, (int)set));
}

int sigsuspend(const sigset_t *mask) {
    return _check(_sc1(SYS_SIGSUSPEND, (int)mask));
}

ssize_t readv(int fd, const struct iovec *iov, int iovcnt) {
    return (ssize_t)_check(_sc3(SYS_READV, fd, (int)iov, iovcnt));
}

ssize_t writev(int fd, const struct iovec *iov, int iovcnt) {
    return (ssize_t)_check(_sc3(SYS_WRITEV, fd, (int)iov, iovcnt));
}

clock_t times(struct tms *buf) {
    return (clock_t)_sc1(SYS_TIMES, (int)buf);
}

int flock(int fd, int operation) {
    return _check(_sc2(SYS_FLOCK, fd, operation));
}

int setitimer(int which, const struct itimerval *new_value, struct itimerval *old_value) {
    return _check(_sc3(SYS_SETITIMER, which, (int)new_value, (int)old_value));
}

int getitimer(int which, struct itimerval *curr_value) {
    return _check(_sc2(SYS_GETITIMER, which, (int)curr_value));
}

int link(const char *oldpath, const char *newpath) {
    return _check(_sc2(SYS_LINK, (int)oldpath, (int)newpath));
}

int symlink(const char *target, const char *linkpath) {
    return _check(_sc2(SYS_SYMLINK, (int)target, (int)linkpath));
}

ssize_t readlink(const char *path, char *buf, size_t bufsiz) {
    return (ssize_t)_check(_sc3(SYS_READLINK, (int)path, (int)buf, (int)bufsiz));
}

int pipe2(int fds[2], int flags) {
    return _check(_sc2(SYS_PIPE2, (int)fds, flags));
}

int clock_gettime(clockid_t clk_id, struct timespec *tp) {
    return _check(_sc2(SYS_CLOCK_GETTIME, (int)clk_id, (int)tp));
}

int gettimeofday(struct timeval *tv, void *tz) {
    (void)tz;
    return _check(_sc2(SYS_GETTIMEOFDAY, (int)tv, 0));
}

void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
    int ret = _sc5(SYS_MMAP, (int)addr, (int)length, prot, flags, fd);
    (void)offset;
    if (ret < 0 && ret > -4096) { errno = -ret; return MAP_FAILED; }
    return (void *)ret;
}

int munmap(void *addr, size_t length) {
    return _check(_sc2(SYS_MUNMAP, (int)addr, (int)length));
}

int mprotect(void *addr, size_t len, int prot) {
    return _check(_sc3(SYS_MPROTECT, (int)addr, (int)len, prot));
}

int madvise(void *addr, size_t length, int advice) {
    return _check(_sc3(SYS_MADVISE, (int)addr, (int)length, advice));
}

int getrlimit(int resource, struct rlimit *rlim) {
    return _check(_sc2(SYS_GETRLIMIT, resource, (int)rlim));
}

int setrlimit(int resource, const struct rlimit *rlim) {
    return _check(_sc2(SYS_SETRLIMIT, resource, (int)rlim));
}

int getrusage(int who, struct rusage *usage) {
    return _check(_sc2(SYS_GETRUSAGE, who, (int)usage));
}

int uname(struct utsname *buf) {
    return _check(_sc1(SYS_UNAME, (int)buf));
}

void *brk(void *addr) {
    return (void *)_sc1(SYS_BRK, (int)addr);
}

void *sbrk(intptr_t increment) {
    void *cur = brk(0);
    if (increment == 0) return cur;
    void *new_end = (void *)((char *)cur + increment);
    void *result = brk(new_end);
    if ((uintptr_t)result < (uintptr_t)new_end) return (void *)-1;
    return cur;
}

int mkfifo(const char *path, mode_t mode) {
    (void)path; (void)mode;
    errno = ENOSYS;
    return -1;
}

/* pwd stubs — return hardcoded root user */
static struct passwd _pw = {"root", "", 0, 0, "", "/", "/bin/sh"};
struct passwd *getpwuid(uid_t uid) { (void)uid; return &_pw; }
struct passwd *getpwnam(const char *name) { (void)name; return &_pw; }

/* Group stubs */
struct group { char *gr_name; char *gr_passwd; gid_t gr_gid; char **gr_mem; };
static struct group _gr = {"root", "", 0, 0};
struct group *getgrent(void) { return &_gr; }
void setgrent(void) {}
void endgrent(void) {}

/* ================================================================
 * Regex stubs (kept as stubs — real regex in Phase 3)
 * ================================================================ */

int regcomp(regex_t *preg, const char *pattern, int cflags) {
    (void)preg; (void)pattern; (void)cflags;
    return -1;
}
int regexec(const regex_t *preg, const char *string, size_t nmatch,
            regmatch_t pmatch[], int eflags) {
    (void)preg; (void)string; (void)nmatch; (void)pmatch; (void)eflags;
    return -1;
}
void regfree(regex_t *preg) { (void)preg; }
size_t regerror(int errcode, const regex_t *preg, char *errbuf, size_t errbuf_size) {
    (void)errcode; (void)preg;
    if (errbuf && errbuf_size > 0) errbuf[0] = '\0';
    return 0;
}

/* fnmatch — basic wildcard matching (* and ? support) */
int fnmatch(const char *pattern, const char *string, int flags) {
    (void)flags;
    const char *p = pattern, *s = string;
    const char *star_p = 0, *star_s = 0;
    while (*s) {
        if (*p == '*') {
            star_p = ++p;
            star_s = s;
            continue;
        }
        if (*p == '?' || *p == *s) { p++; s++; continue; }
        if (star_p) { p = star_p; s = ++star_s; continue; }
        return 1; /* FNM_NOMATCH */
    }
    while (*p == '*') p++;
    return *p ? 1 : 0;
}
