#include <stdint.h>

#ifdef SIGKILL
#undef SIGKILL
#endif
#ifdef SIGUSR1
#undef SIGUSR1
#endif
#ifdef SIGSEGV
#undef SIGSEGV
#endif
#ifdef SIGTTIN
#undef SIGTTIN
#endif
#ifdef SIGTTOU
#undef SIGTTOU
#endif

#ifdef WNOHANG
#undef WNOHANG
#endif
#ifdef SEEK_SET
#undef SEEK_SET
#endif
#ifdef SEEK_CUR
#undef SEEK_CUR
#endif
#ifdef SEEK_END
#undef SEEK_END
#endif

#include "user_errno.h"

#include "signal.h"

enum {
    SYSCALL_WRITE = 1,
    SYSCALL_EXIT  = 2,
    SYSCALL_GETPID = 3,
    SYSCALL_OPEN  = 4,
    SYSCALL_READ  = 5,
    SYSCALL_CLOSE = 6,
    SYSCALL_WAITPID = 7,
    SYSCALL_LSEEK = 9,
    SYSCALL_FSTAT = 10,
    SYSCALL_STAT = 11,

    SYSCALL_DUP = 12,
    SYSCALL_DUP2 = 13,
    SYSCALL_PIPE = 14,
    SYSCALL_PIPE2 = 34,
    SYSCALL_EXECVE = 15,
    SYSCALL_FORK = 16,
    SYSCALL_GETPPID = 17,
    SYSCALL_POLL = 18,
    SYSCALL_KILL = 19,
    SYSCALL_SELECT = 20,
    SYSCALL_IOCTL = 21,
    SYSCALL_SETSID = 22,
    SYSCALL_SETPGID = 23,
    SYSCALL_GETPGRP = 24,

    SYSCALL_SIGACTION = 25,
    SYSCALL_SIGPROCMASK = 26,
    SYSCALL_SIGRETURN = 27,

    SYSCALL_MKDIR = 28,
    SYSCALL_UNLINK = 29,

    SYSCALL_GETDENTS = 30,

    SYSCALL_FCNTL = 31,

    SYSCALL_CHDIR = 32,
    SYSCALL_GETCWD = 33,
    SYSCALL_DUP3 = 35,

    SYSCALL_OPENAT = 36,
    SYSCALL_FSTATAT = 37,
    SYSCALL_UNLINKAT = 38,

    SYSCALL_RENAME = 39,
    SYSCALL_RMDIR = 40,
};

enum {
    AT_FDCWD = -100,
};

enum {
    F_GETFL = 3,
    F_SETFL = 4,
};

enum {
    TCGETS = 0x5401,
    TCSETS = 0x5402,
    TIOCGPGRP = 0x540F,
    TIOCSPGRP = 0x5410,
};

enum {
    ENOTTY = 25,
};

enum {
    ICANON = 0x0001,
    ECHO   = 0x0002,
};

struct termios {
    uint32_t c_lflag;
};

struct pollfd {
    int fd;
    int16_t events;
    int16_t revents;
};

enum {
    POLLIN = 0x0001,
    POLLOUT = 0x0004,
};

enum {
    SIGKILL = 9,
    SIGUSR1 = 10,
    SIGSEGV = 11,
    SIGTTIN = 21,
    SIGTTOU = 22,
};

enum {
    WNOHANG = 1,
};

enum {
    SEEK_SET = 0,
    SEEK_CUR = 1,
    SEEK_END = 2,
};

enum {
    O_CREAT = 0x40,
    O_TRUNC = 0x200,
    O_NONBLOCK = 0x800,
};

enum {
    EAGAIN = 11,
    EINVAL = 22,
};

#define S_IFMT  0170000
#define S_IFREG 0100000

struct stat {
    uint32_t st_ino;
    uint32_t st_mode;
    uint32_t st_nlink;
    uint32_t st_size;
};

static int sys_write(int fd, const void* buf, uint32_t len) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_WRITE), "b"(fd), "c"(buf), "d"(len)
        : "memory"
    );
    return __syscall_fix(ret);
}

static int sys_openat(int dirfd, const char* path, uint32_t flags, uint32_t mode) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_OPENAT), "b"(dirfd), "c"(path), "d"(flags), "S"(mode)
        : "memory"
    );
    return __syscall_fix(ret);
}

static int sys_fstatat(int dirfd, const char* path, struct stat* st, uint32_t flags) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_FSTATAT), "b"(dirfd), "c"(path), "d"(st), "S"(flags)
        : "memory"
    );
    return __syscall_fix(ret);
}

static int sys_unlinkat(int dirfd, const char* path, uint32_t flags) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_UNLINKAT), "b"(dirfd), "c"(path), "d"(flags)
        : "memory"
    );
    return __syscall_fix(ret);
}

static int sys_rename(const char* oldpath, const char* newpath) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_RENAME), "b"(oldpath), "c"(newpath)
        : "memory"
    );
    return __syscall_fix(ret);
}

static int sys_rmdir(const char* path) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_RMDIR), "b"(path)
        : "memory"
    );
    return __syscall_fix(ret);
}

static int sys_pipe2(int fds[2], uint32_t flags) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_PIPE2), "b"(fds), "c"(flags)
        : "memory"
    );
    return __syscall_fix(ret);
}

static int sys_chdir(const char* path) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_CHDIR), "b"(path)
        : "memory"
    );
    return __syscall_fix(ret);
}

static int sys_getcwd(char* buf, uint32_t size) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_GETCWD), "b"(buf), "c"(size)
        : "memory"
    );
    return __syscall_fix(ret);
}

static int sys_fcntl(int fd, int cmd, uint32_t arg) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_FCNTL), "b"(fd), "c"(cmd), "d"(arg)
        : "memory"
    );
    return __syscall_fix(ret);
}

static int sys_ioctl(int fd, uint32_t cmd, void* arg);

static int isatty_fd(int fd) {
    struct termios t;
    if (sys_ioctl(fd, TCGETS, &t) < 0) {
        if (errno == ENOTTY) return 0;
        return -1;
    }
    return 1;
}

static int sys_getdents(int fd, void* buf, uint32_t len) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_GETDENTS), "b"(fd), "c"(buf), "d"(len)
        : "memory"
    );
    return __syscall_fix(ret);
}

static void write_int_dec(int v) {
    char buf[16];
    int i = 0;
    if (v == 0) {
        buf[i++] = '0';
    } else {
        int neg = 0;
        if (v < 0) {
            neg = 1;
            v = -v;
        }
        while (v > 0 && i < (int)sizeof(buf)) {
            buf[i++] = (char)('0' + (v % 10));
            v /= 10;
        }
        if (neg && i < (int)sizeof(buf)) {
            buf[i++] = '-';
        }
        for (int j = 0; j < i / 2; j++) {
            char t = buf[j];
            buf[j] = buf[i - 1 - j];
            buf[i - 1 - j] = t;
        }
    }
    (void)sys_write(1, buf, (uint32_t)i);
}

static void write_hex8(uint8_t v) {
    static const char hex[] = "0123456789ABCDEF";
    char b[2];
    b[0] = hex[(v >> 4) & 0xF];
    b[1] = hex[v & 0xF];
    (void)sys_write(1, b, 2);
}

static void write_hex32(uint32_t v) {
    static const char hex[] = "0123456789ABCDEF";
    char b[8];
    for (int i = 0; i < 8; i++) {
        uint32_t shift = (uint32_t)(28 - 4 * i);
        b[i] = hex[(v >> shift) & 0xFU];
    }
    (void)sys_write(1, b, 8);
}

static int memeq(const void* a, const void* b, uint32_t n) {
    const uint8_t* x = (const uint8_t*)a;
    const uint8_t* y = (const uint8_t*)b;
    for (uint32_t i = 0; i < n; i++) {
        if (x[i] != y[i]) return 0;
    }
    return 1;
}

static int streq(const char* a, const char* b) {
    if (!a || !b) return 0;
    uint32_t i = 0;
    while (a[i] != 0 && b[i] != 0) {
        if (a[i] != b[i]) return 0;
        i++;
    }
    return a[i] == b[i];
}

static int sys_sigaction2(int sig, const struct sigaction* act, struct sigaction* oldact) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_SIGACTION), "b"(sig), "c"(act), "d"(oldact)
        : "memory"
    );
    return __syscall_fix(ret);
}

static int sys_sigaction(int sig, void (*handler)(int), uintptr_t* old_out) {
    struct sigaction act;
    act.sa_handler = (uintptr_t)handler;
    act.sa_sigaction = 0;
    act.sa_mask = 0;
    act.sa_flags = 0;

    struct sigaction oldact;
    struct sigaction* oldp = old_out ? &oldact : 0;

    int r = sys_sigaction2(sig, &act, oldp);
    if (r < 0) return r;
    if (old_out) {
        *old_out = oldact.sa_handler;
    }
    return 0;
}

static int sys_select(uint32_t nfds, uint64_t* readfds, uint64_t* writefds, uint64_t* exceptfds, int32_t timeout) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_SELECT), "b"(nfds), "c"(readfds), "d"(writefds), "S"(exceptfds), "D"(timeout)
        : "memory"
    );
    return __syscall_fix(ret);
}

static int sys_ioctl(int fd, uint32_t cmd, void* arg) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_IOCTL), "b"(fd), "c"(cmd), "d"(arg)
        : "memory"
    );
    return __syscall_fix(ret);
}

static int sys_kill(int pid, int sig) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_KILL), "b"(pid), "c"(sig)
        : "memory"
    );
    return __syscall_fix(ret);
}

static int sys_poll(struct pollfd* fds, uint32_t nfds, int32_t timeout) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_POLL), "b"(fds), "c"(nfds), "d"(timeout)
        : "memory"
    );
    return __syscall_fix(ret);
}

static int sys_setsid(void) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_SETSID)
        : "memory"
    );
    return __syscall_fix(ret);
}

static int sys_setpgid(int pid, int pgid) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_SETPGID), "b"(pid), "c"(pgid)
        : "memory"
    );
    return __syscall_fix(ret);
}

static int sys_getpgrp(void) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_GETPGRP)
        : "memory"
    );
    return __syscall_fix(ret);
}

static int sys_getpid(void) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_GETPID)
        : "memory"
    );
    return __syscall_fix(ret);
}

static int sys_getppid(void) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_GETPPID)
        : "memory"
    );
    return __syscall_fix(ret);
}

static int sys_fork(void) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_FORK)
        : "memory"
    );
    return __syscall_fix(ret);
}

static int sys_execve(const char* path, const char* const* argv, const char* const* envp) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_EXECVE), "b"(path), "c"(argv), "d"(envp)
        : "memory"
    );
    return __syscall_fix(ret);
}

static int sys_pipe(int fds[2]) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_PIPE), "b"(fds)
        : "memory"
    );
    return __syscall_fix(ret);
}

static int sys_dup(int oldfd) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_DUP), "b"(oldfd)
        : "memory"
    );
    return __syscall_fix(ret);
}

static int sys_dup2(int oldfd, int newfd) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_DUP2), "b"(oldfd), "c"(newfd)
        : "memory"
    );
    return __syscall_fix(ret);
}

static int sys_dup3(int oldfd, int newfd, uint32_t flags) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_DUP3), "b"(oldfd), "c"(newfd), "d"(flags)
        : "memory"
    );
    return __syscall_fix(ret);
}

static int sys_waitpid(int pid, int* status, uint32_t options) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_WAITPID), "b"(pid), "c"(status), "d"(options)
        : "memory"
    );
    return __syscall_fix(ret);
}

static int sys_open(const char* path, uint32_t flags) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_OPEN), "b"(path), "c"(flags)
        : "memory"
    );
    return __syscall_fix(ret);
}

static int sys_mkdir(const char* path) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_MKDIR), "b"(path)
        : "memory"
    );
    return __syscall_fix(ret);
}

static int sys_unlink(const char* path) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_UNLINK), "b"(path)
        : "memory"
    );
    return __syscall_fix(ret);
}

static int sys_read(int fd, void* buf, uint32_t len) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_READ), "b"(fd), "c"(buf), "d"(len)
        : "memory"
    );
    return __syscall_fix(ret);
}

static int sys_close(int fd) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_CLOSE), "b"(fd)
        : "memory"
    );
    return __syscall_fix(ret);
}

static int sys_lseek(int fd, int32_t offset, int whence) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_LSEEK), "b"(fd), "c"(offset), "d"(whence)
        : "memory"
    );
    return __syscall_fix(ret);
}

static int sys_fstat(int fd, struct stat* st) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_FSTAT), "b"(fd), "c"(st)
        : "memory"
    );
    return __syscall_fix(ret);
}

static int sys_stat(const char* path, struct stat* st) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_STAT), "b"(path), "c"(st)
        : "memory"
    );
    return __syscall_fix(ret);
}

__attribute__((noreturn)) static void sys_exit(int code) {
    __asm__ volatile(
        "int $0x80\n"
        "1: jmp 1b\n"
        :
        : "a"(SYSCALL_EXIT), "b"(code)
        : "memory"
    );
    for (;;) {
        __asm__ volatile("hlt");
    }
}

static volatile int got_usr1 = 0;
static volatile int got_usr1_ret = 0;
static volatile int got_ttin = 0;
static volatile int got_ttou = 0;

static void usr1_handler(int sig) {
    (void)sig;
    got_usr1 = 1;
    sys_write(1, "[init] SIGUSR1 handler OK\n",
              (uint32_t)(sizeof("[init] SIGUSR1 handler OK\n") - 1));
}

static void usr1_ret_handler(int sig) {
    (void)sig;
    got_usr1_ret = 1;
}

static void ttin_handler(int sig) {
    (void)sig;
    got_ttin = 1;
}

static void ttou_handler(int sig) {
    (void)sig;
    got_ttou = 1;
}

static void sigsegv_exit_handler(int sig) {
    (void)sig;
    static const char msg[] = "[init] SIGSEGV handler invoked\n";
    (void)sys_write(1, msg, (uint32_t)(sizeof(msg) - 1));
    sys_exit(0);
}

static void sigsegv_info_handler(int sig, siginfo_t* info, void* uctx) {
    (void)uctx;
    static const char msg[] = "[init] SIGSEGV siginfo handler invoked\n";
    (void)sys_write(1, msg, (uint32_t)(sizeof(msg) - 1));
    const uintptr_t expected = 0x12345000U;
    if (sig == SIGSEGV && info && (uintptr_t)info->si_addr == expected) {
        sys_exit(0);
    }
    sys_exit(1);
}

void _start(void) {
    __asm__ volatile(
        "mov $0x23, %ax\n"
        "mov %ax, %ds\n"
        "mov %ax, %es\n"
        "mov %ax, %fs\n"
        "mov %ax, %gs\n"
    );

    static const char msg[] = "[init] hello from init.elf\n";
    (void)sys_write(1, msg, (uint32_t)(sizeof(msg) - 1));

    static const char path[] = "/bin/init.elf";

    int fd = sys_open(path, 0);
    if (fd < 0) {
        sys_write(1, "[init] open failed fd=", (uint32_t)(sizeof("[init] open failed fd=") - 1));
        write_int_dec(fd);
        sys_write(1, "\n", 1);
        sys_exit(1);
    }

    uint8_t hdr[4];
    int rd = sys_read(fd, hdr, 4);
    (void)sys_close(fd);
    if (rd == 4 && hdr[0] == 0x7F && hdr[1] == 'E' && hdr[2] == 'L' && hdr[3] == 'F') {
        sys_write(1, "[init] open/read/close OK (ELF magic)\n",
                  (uint32_t)(sizeof("[init] open/read/close OK (ELF magic)\n") - 1));
    } else {
        sys_write(1, "[init] read failed or bad header rd=", (uint32_t)(sizeof("[init] read failed or bad header rd=") - 1));
        write_int_dec(rd);
        sys_write(1, " hdr=", (uint32_t)(sizeof(" hdr=") - 1));
        for (int i = 0; i < 4; i++) {
            write_hex8(hdr[i]);
        }
        sys_write(1, "\n", 1);
        sys_exit(1);
    }

    fd = sys_open("/bin/init.elf", 0);
    if (fd < 0) {
        sys_write(1, "[init] overlay open failed\n",
                  (uint32_t)(sizeof("[init] overlay open failed\n") - 1));
        sys_exit(1);
    }

    uint8_t orig0 = 0;
    if (sys_lseek(fd, 0, SEEK_SET) < 0 || sys_read(fd, &orig0, 1) != 1) {
        sys_write(1, "[init] overlay read failed\n",
                  (uint32_t)(sizeof("[init] overlay read failed\n") - 1));
        sys_exit(1);
    }

    uint8_t x = (uint8_t)(orig0 ^ 0xFF);
    if (sys_lseek(fd, 0, SEEK_SET) < 0 || sys_write(fd, &x, 1) != 1) {
        sys_write(1, "[init] overlay write failed\n",
                  (uint32_t)(sizeof("[init] overlay write failed\n") - 1));
        sys_exit(1);
    }

    if (sys_close(fd) < 0) {
        sys_write(1, "[init] overlay close failed\n",
                  (uint32_t)(sizeof("[init] overlay close failed\n") - 1));
        sys_exit(1);
    }

    fd = sys_open("/bin/init.elf", 0);
    if (fd < 0) {
        sys_write(1, "[init] overlay open2 failed\n",
                  (uint32_t)(sizeof("[init] overlay open2 failed\n") - 1));
        sys_exit(1);
    }

    uint8_t chk = 0;
    if (sys_lseek(fd, 0, SEEK_SET) < 0 || sys_read(fd, &chk, 1) != 1 || chk != x) {
        sys_write(1, "[init] overlay verify failed\n",
                  (uint32_t)(sizeof("[init] overlay verify failed\n") - 1));
        sys_exit(1);
    }

    if (sys_lseek(fd, 0, SEEK_SET) < 0 || sys_write(fd, &orig0, 1) != 1) {
        sys_write(1, "[init] overlay restore failed\n",
                  (uint32_t)(sizeof("[init] overlay restore failed\n") - 1));
        sys_exit(1);
    }

    if (sys_close(fd) < 0) {
        sys_write(1, "[init] overlay close2 failed\n",
                  (uint32_t)(sizeof("[init] overlay close2 failed\n") - 1));
        sys_exit(1);
    }

    sys_write(1, "[init] overlay copy-up OK\n",
              (uint32_t)(sizeof("[init] overlay copy-up OK\n") - 1));

    fd = sys_open("/bin/init.elf", 0);
    if (fd < 0) {
        sys_write(1, "[init] open2 failed\n", (uint32_t)(sizeof("[init] open2 failed\n") - 1));
        sys_exit(1);
    }

    struct stat st;
    if (sys_fstat(fd, &st) < 0) {
        sys_write(1, "[init] fstat failed\n", (uint32_t)(sizeof("[init] fstat failed\n") - 1));
        sys_exit(1);
    }

    if ((st.st_mode & S_IFMT) != S_IFREG || st.st_size == 0) {
        sys_write(1, "[init] fstat bad\n", (uint32_t)(sizeof("[init] fstat bad\n") - 1));
        sys_exit(1);
    }

    if (sys_lseek(fd, 0, SEEK_SET) < 0) {
        sys_write(1, "[init] lseek set failed\n",
                  (uint32_t)(sizeof("[init] lseek set failed\n") - 1));
        sys_exit(1);
    }

    uint8_t m2[4];
    if (sys_read(fd, m2, 4) != 4) {
        sys_write(1, "[init] read2 failed\n", (uint32_t)(sizeof("[init] read2 failed\n") - 1));
        sys_exit(1);
    }
    if (m2[0] != 0x7F || m2[1] != 'E' || m2[2] != 'L' || m2[3] != 'F') {
        sys_write(1, "[init] lseek/read mismatch\n",
                  (uint32_t)(sizeof("[init] lseek/read mismatch\n") - 1));
        sys_exit(1);
    }

    if (sys_close(fd) < 0) {
        sys_write(1, "[init] close2 failed\n", (uint32_t)(sizeof("[init] close2 failed\n") - 1));
        sys_exit(1);
    }

    if (sys_stat("/bin/init.elf", &st) < 0) {
        sys_write(1, "[init] stat failed\n", (uint32_t)(sizeof("[init] stat failed\n") - 1));
        sys_exit(1);
    }
    if ((st.st_mode & S_IFMT) != S_IFREG || st.st_size == 0) {
        sys_write(1, "[init] stat bad\n", (uint32_t)(sizeof("[init] stat bad\n") - 1));
        sys_exit(1);
    }

    sys_write(1, "[init] lseek/stat/fstat OK\n",
              (uint32_t)(sizeof("[init] lseek/stat/fstat OK\n") - 1));

    fd = sys_open("/tmp/hello.txt", 0);
    if (fd < 0) {
        sys_write(1, "[init] tmpfs open failed\n",
                  (uint32_t)(sizeof("[init] tmpfs open failed\n") - 1));
        sys_exit(1);
    }

    if (sys_lseek(fd, 0, SEEK_END) < 0) {
        sys_write(1, "[init] dup2 prep lseek failed\n",
                  (uint32_t)(sizeof("[init] dup2 prep lseek failed\n") - 1));
        sys_exit(1);
    }

    if (sys_dup2(fd, 1) != 1) {
        sys_write(1, "[init] dup2 failed\n", (uint32_t)(sizeof("[init] dup2 failed\n") - 1));
        sys_exit(1);
    }

    (void)sys_close(fd);

    {
        static const char m[] = "[init] dup2 stdout->file OK\n";
        if (sys_write(1, m, (uint32_t)(sizeof(m) - 1)) != (int)(sizeof(m) - 1)) {
            sys_exit(1);
        }
    }

    (void)sys_close(1);
    sys_write(1, "[init] dup2 restore tty OK\n",
              (uint32_t)(sizeof("[init] dup2 restore tty OK\n") - 1));

    {
        int pfds[2];
        if (sys_pipe(pfds) < 0) {
            sys_write(1, "[init] pipe failed\n", (uint32_t)(sizeof("[init] pipe failed\n") - 1));
            sys_exit(1);
        }

        static const char pmsg[] = "pipe-test";
        if (sys_write(pfds[1], pmsg, (uint32_t)(sizeof(pmsg) - 1)) != (int)(sizeof(pmsg) - 1)) {
            sys_write(1, "[init] pipe write failed\n",
                      (uint32_t)(sizeof("[init] pipe write failed\n") - 1));
            sys_exit(1);
        }

        char rbuf[16];
        int prd = sys_read(pfds[0], rbuf, (uint32_t)(sizeof(pmsg) - 1));
        if (prd != (int)(sizeof(pmsg) - 1)) {
            sys_write(1, "[init] pipe read failed\n",
                      (uint32_t)(sizeof("[init] pipe read failed\n") - 1));
            sys_exit(1);
        }

        int ok = 1;
        for (uint32_t i = 0; i < (uint32_t)(sizeof(pmsg) - 1); i++) {
            if ((uint8_t)rbuf[i] != (uint8_t)pmsg[i]) ok = 0;
        }
        if (!ok) {
            sys_write(1, "[init] pipe mismatch\n",
                      (uint32_t)(sizeof("[init] pipe mismatch\n") - 1));
            sys_exit(1);
        }

        if (sys_dup2(pfds[1], 1) != 1) {
            sys_write(1, "[init] pipe dup2 failed\n",
                      (uint32_t)(sizeof("[init] pipe dup2 failed\n") - 1));
            sys_exit(1);
        }

        static const char p2[] = "dup2-pipe";
        if (sys_write(1, p2, (uint32_t)(sizeof(p2) - 1)) != (int)(sizeof(p2) - 1)) {
            sys_exit(1);
        }

        int prd2 = sys_read(pfds[0], rbuf, (uint32_t)(sizeof(p2) - 1));
        if (prd2 != (int)(sizeof(p2) - 1)) {
            sys_write(1, "[init] pipe dup2 read failed\n",
                      (uint32_t)(sizeof("[init] pipe dup2 read failed\n") - 1));
            sys_exit(1);
        }
        sys_write(1, "[init] pipe OK\n", (uint32_t)(sizeof("[init] pipe OK\n") - 1));

        (void)sys_close(pfds[0]);
        (void)sys_close(pfds[1]);

        int tfd = sys_open("/dev/tty", 0);
        if (tfd < 0) {
            sys_write(1, "[init] /dev/tty open failed\n",
                      (uint32_t)(sizeof("[init] /dev/tty open failed\n") - 1));
            sys_exit(1);
        }
        if (sys_dup2(tfd, 1) != 1) {
            sys_write(1, "[init] dup2 restore tty failed\n",
                      (uint32_t)(sizeof("[init] dup2 restore tty failed\n") - 1));
            sys_exit(1);
        }
        (void)sys_close(tfd);

    }

    {
        int pid = sys_fork();
        if (pid < 0) {
            sys_write(1, "[init] kill test fork failed\n",
                      (uint32_t)(sizeof("[init] kill test fork failed\n") - 1));
            sys_exit(1);
        }

        if (pid == 0) {
            for (;;) {
                __asm__ volatile("nop");
            }
        }

        if (sys_kill(pid, SIGKILL) < 0) {
            sys_write(1, "[init] kill(SIGKILL) failed\n",
                      (uint32_t)(sizeof("[init] kill(SIGKILL) failed\n") - 1));
            sys_exit(1);
        }

        int st = 0;
        int rp = sys_waitpid(pid, &st, 0);
        if (rp != pid || st != (128 + SIGKILL)) {
            sys_write(1, "[init] kill test waitpid mismatch\n",
                      (uint32_t)(sizeof("[init] kill test waitpid mismatch\n") - 1));
            sys_exit(1);
        }

        sys_write(1, "[init] kill(SIGKILL) OK\n",
                  (uint32_t)(sizeof("[init] kill(SIGKILL) OK\n") - 1));
    }

    {
        int fds[2];
        if (sys_pipe(fds) < 0) {
            sys_write(1, "[init] poll pipe setup failed\n",
                      (uint32_t)(sizeof("[init] poll pipe setup failed\n") - 1));
            sys_exit(1);
        }

        struct pollfd p;
        p.fd = fds[0];
        p.events = POLLIN;
        p.revents = 0;
        int rc = sys_poll(&p, 1, 0);
        if (rc != 0) {
            sys_write(1, "[init] poll(pipe) expected 0\n",
                      (uint32_t)(sizeof("[init] poll(pipe) expected 0\n") - 1));
            sys_exit(1);
        }

        static const char a = 'A';
        if (sys_write(fds[1], &a, 1) != 1) {
            sys_write(1, "[init] poll pipe write failed\n",
                      (uint32_t)(sizeof("[init] poll pipe write failed\n") - 1));
            sys_exit(1);
        }

        p.revents = 0;
        rc = sys_poll(&p, 1, 0);
        if (rc != 1 || (p.revents & POLLIN) == 0) {
            sys_write(1, "[init] poll(pipe) expected POLLIN\n",
                      (uint32_t)(sizeof("[init] poll(pipe) expected POLLIN\n") - 1));
            sys_exit(1);
        }

        (void)sys_close(fds[0]);
        (void)sys_close(fds[1]);
        sys_write(1, "[init] poll(pipe) OK\n", (uint32_t)(sizeof("[init] poll(pipe) OK\n") - 1));
    }

    {
        int fds[2];
        if (sys_pipe(fds) < 0) {
            sys_write(1, "[init] select pipe setup failed\n",
                      (uint32_t)(sizeof("[init] select pipe setup failed\n") - 1));
            sys_exit(1);
        }

        uint64_t r = 0;
        uint64_t w = 0;
        r |= (1ULL << (uint32_t)fds[0]);
        int rc = sys_select((uint32_t)(fds[0] + 1), &r, &w, 0, 0);
        if (rc != 0) {
            sys_write(1, "[init] select(pipe) expected 0\n",
                      (uint32_t)(sizeof("[init] select(pipe) expected 0\n") - 1));
            sys_exit(1);
        }

        static const char a = 'B';
        if (sys_write(fds[1], &a, 1) != 1) {
            sys_write(1, "[init] select pipe write failed\n",
                      (uint32_t)(sizeof("[init] select pipe write failed\n") - 1));
            sys_exit(1);
        }

        r = 0;
        w = 0;
        r |= (1ULL << (uint32_t)fds[0]);
        rc = sys_select((uint32_t)(fds[0] + 1), &r, &w, 0, 0);
        if (rc != 1 || ((r >> (uint32_t)fds[0]) & 1ULL) == 0) {
            sys_write(1, "[init] select(pipe) expected readable\n",
                      (uint32_t)(sizeof("[init] select(pipe) expected readable\n") - 1));
            sys_exit(1);
        }

        (void)sys_close(fds[0]);
        (void)sys_close(fds[1]);
        sys_write(1, "[init] select(pipe) OK\n",
                  (uint32_t)(sizeof("[init] select(pipe) OK\n") - 1));
    }

    {
        int fd = sys_open("/dev/tty", 0);
        if (fd < 0) {
            sys_write(1, "[init] ioctl(/dev/tty) open failed\n",
                      (uint32_t)(sizeof("[init] ioctl(/dev/tty) open failed\n") - 1));
            sys_exit(1);
        }

        int fg = -1;
        if (sys_ioctl(fd, TIOCGPGRP, &fg) < 0 || fg != 0) {
            sys_write(1, "[init] ioctl TIOCGPGRP failed\n",
                      (uint32_t)(sizeof("[init] ioctl TIOCGPGRP failed\n") - 1));
            sys_exit(1);
        }

        fg = 0;
        if (sys_ioctl(fd, TIOCSPGRP, &fg) < 0) {
            sys_write(1, "[init] ioctl TIOCSPGRP failed\n",
                      (uint32_t)(sizeof("[init] ioctl TIOCSPGRP failed\n") - 1));
            sys_exit(1);
        }

        fg = 1;
        if (sys_ioctl(fd, TIOCSPGRP, &fg) >= 0) {
            sys_write(1, "[init] ioctl TIOCSPGRP expected fail\n",
                      (uint32_t)(sizeof("[init] ioctl TIOCSPGRP expected fail\n") - 1));
            sys_exit(1);
        }

        struct termios oldt;
        if (sys_ioctl(fd, TCGETS, &oldt) < 0) {
            sys_write(1, "[init] ioctl TCGETS failed\n",
                      (uint32_t)(sizeof("[init] ioctl TCGETS failed\n") - 1));
            sys_exit(1);
        }

        struct termios t = oldt;
        t.c_lflag &= ~(uint32_t)(ECHO | ICANON);
        if (sys_ioctl(fd, TCSETS, &t) < 0) {
            sys_write(1, "[init] ioctl TCSETS failed\n",
                      (uint32_t)(sizeof("[init] ioctl TCSETS failed\n") - 1));
            sys_exit(1);
        }

        struct termios chk;
        if (sys_ioctl(fd, TCGETS, &chk) < 0) {
            sys_write(1, "[init] ioctl TCGETS2 failed\n",
                      (uint32_t)(sizeof("[init] ioctl TCGETS2 failed\n") - 1));
            sys_exit(1);
        }

        if ((chk.c_lflag & (uint32_t)(ECHO | ICANON)) != 0) {
            sys_write(1, "[init] ioctl verify failed\n",
                      (uint32_t)(sizeof("[init] ioctl verify failed\n") - 1));
            sys_exit(1);
        }

        (void)sys_ioctl(fd, TCSETS, &oldt);
        (void)sys_close(fd);

        sys_write(1, "[init] ioctl(/dev/tty) OK\n",
                  (uint32_t)(sizeof("[init] ioctl(/dev/tty) OK\n") - 1));
    }

    // A2: basic job control. A background pgrp read/write on controlling TTY should raise SIGTTIN/SIGTTOU.
    {
        int leader = sys_fork();
        if (leader < 0) {
            sys_write(1, "[init] fork(job control leader) failed\n",
                      (uint32_t)(sizeof("[init] fork(job control leader) failed\n") - 1));
            sys_exit(1);
        }
        if (leader == 0) {
            int me = sys_getpid();
            int sid = sys_setsid();
            if (sid != me) {
                sys_write(1, "[init] setsid(job control) failed\n",
                          (uint32_t)(sizeof("[init] setsid(job control) failed\n") - 1));
                sys_exit(1);
            }

            int tfd = sys_open("/dev/tty", 0);
            if (tfd < 0) {
                sys_write(1, "[init] open(/dev/tty) for job control failed\n",
                          (uint32_t)(sizeof("[init] open(/dev/tty) for job control failed\n") - 1));
                sys_exit(1);
            }

            // Touch ioctl to make kernel acquire controlling session/pgrp.
            int fg = 0;
            (void)sys_ioctl(tfd, TIOCGPGRP, &fg);

            fg = me;
            if (sys_ioctl(tfd, TIOCSPGRP, &fg) < 0) {
                sys_write(1, "[init] ioctl TIOCSPGRP(job control) failed\n",
                          (uint32_t)(sizeof("[init] ioctl TIOCSPGRP(job control) failed\n") - 1));
                sys_exit(1);
            }

            int bg = sys_fork();
            if (bg < 0) {
                sys_write(1, "[init] fork(job control bg) failed\n",
                          (uint32_t)(sizeof("[init] fork(job control bg) failed\n") - 1));
                sys_exit(1);
            }
            if (bg == 0) {
                (void)sys_setpgid(0, me + 1);

                (void)sys_sigaction(SIGTTIN, ttin_handler, 0);
                (void)sys_sigaction(SIGTTOU, ttou_handler, 0);

                uint8_t b = 0;
                (void)sys_read(tfd, &b, 1);
                if (!got_ttin) {
                    sys_write(1, "[init] SIGTTIN job control failed\n",
                              (uint32_t)(sizeof("[init] SIGTTIN job control failed\n") - 1));
                    sys_exit(1);
                }

                const char msg2[] = "x";
                (void)sys_write(tfd, msg2, 1);
                if (!got_ttou) {
                    sys_write(1, "[init] SIGTTOU job control failed\n",
                              (uint32_t)(sizeof("[init] SIGTTOU job control failed\n") - 1));
                    sys_exit(1);
                }

                sys_exit(0);
            }

            int st2 = 0;
            int wp2 = sys_waitpid(bg, &st2, 0);
            if (wp2 != bg || st2 != 0) {
                sys_write(1, "[init] waitpid(job control bg) failed wp=", (uint32_t)(sizeof("[init] waitpid(job control bg) failed wp=") - 1));
                write_int_dec(wp2);
                sys_write(1, " st=", (uint32_t)(sizeof(" st=") - 1));
                write_int_dec(st2);
                sys_write(1, "\n", 1);
                sys_exit(1);
            }

            (void)sys_close(tfd);
            sys_exit(0);
        }

        int stL = 0;
        int wpL = sys_waitpid(leader, &stL, 0);
        if (wpL != leader || stL != 0) {
            sys_write(1, "[init] waitpid(job control leader) failed wp=", (uint32_t)(sizeof("[init] waitpid(job control leader) failed wp=") - 1));
            write_int_dec(wpL);
            sys_write(1, " st=", (uint32_t)(sizeof(" st=") - 1));
            write_int_dec(stL);
            sys_write(1, "\n", 1);
            sys_exit(1);
        }

        sys_write(1, "[init] job control (SIGTTIN/SIGTTOU) OK\n",
                  (uint32_t)(sizeof("[init] job control (SIGTTIN/SIGTTOU) OK\n") - 1));
    }

    {
        int fd = sys_open("/dev/null", 0);
        if (fd < 0) {
            sys_write(1, "[init] poll(/dev/null) open failed\n",
                      (uint32_t)(sizeof("[init] poll(/dev/null) open failed\n") - 1));
            sys_exit(1);
        }
        struct pollfd p;
        p.fd = fd;
        p.events = POLLOUT;
        p.revents = 0;
        int rc = sys_poll(&p, 1, 0);
        if (rc != 1 || (p.revents & POLLOUT) == 0) {
            sys_write(1, "[init] poll(/dev/null) expected POLLOUT\n",
                      (uint32_t)(sizeof("[init] poll(/dev/null) expected POLLOUT\n") - 1));
            sys_exit(1);
        }
        (void)sys_close(fd);
        sys_write(1, "[init] poll(/dev/null) OK\n",
                  (uint32_t)(sizeof("[init] poll(/dev/null) OK\n") - 1));
    }

    {
        int mfd = sys_open("/dev/ptmx", 0);
        int sfd = sys_open("/dev/pts/0", 0);
        if (mfd < 0 || sfd < 0) {
            sys_write(1, "[init] pty open failed\n",
                      (uint32_t)(sizeof("[init] pty open failed\n") - 1));
            sys_exit(1);
        }

        static const char m2s[] = "m2s";
        if (sys_write(mfd, m2s, (uint32_t)(sizeof(m2s) - 1)) != (int)(sizeof(m2s) - 1)) {
            sys_write(1, "[init] pty write master failed\n",
                      (uint32_t)(sizeof("[init] pty write master failed\n") - 1));
            sys_exit(1);
        }

        struct pollfd p;
        p.fd = sfd;
        p.events = POLLIN;
        p.revents = 0;
        int rc = sys_poll(&p, 1, 50);
        if (rc != 1 || (p.revents & POLLIN) == 0) {
            sys_write(1, "[init] pty poll slave failed\n",
                      (uint32_t)(sizeof("[init] pty poll slave failed\n") - 1));
            sys_exit(1);
        }

        char buf[8];
        int rd = sys_read(sfd, buf, (uint32_t)(sizeof(m2s) - 1));
        if (rd != (int)(sizeof(m2s) - 1) || !memeq(buf, m2s, (uint32_t)(sizeof(m2s) - 1))) {
            sys_write(1, "[init] pty read slave failed\n",
                      (uint32_t)(sizeof("[init] pty read slave failed\n") - 1));
            sys_exit(1);
        }

        static const char s2m[] = "s2m";
        if (sys_write(sfd, s2m, (uint32_t)(sizeof(s2m) - 1)) != (int)(sizeof(s2m) - 1)) {
            sys_write(1, "[init] pty write slave failed\n",
                      (uint32_t)(sizeof("[init] pty write slave failed\n") - 1));
            sys_exit(1);
        }

        p.fd = mfd;
        p.events = POLLIN;
        p.revents = 0;
        rc = sys_poll(&p, 1, 50);
        if (rc != 1 || (p.revents & POLLIN) == 0) {
            sys_write(1, "[init] pty poll master failed\n",
                      (uint32_t)(sizeof("[init] pty poll master failed\n") - 1));
            sys_exit(1);
        }

        rd = sys_read(mfd, buf, (uint32_t)(sizeof(s2m) - 1));
        if (rd != (int)(sizeof(s2m) - 1) || !memeq(buf, s2m, (uint32_t)(sizeof(s2m) - 1))) {
            sys_write(1, "[init] pty read master failed\n",
                      (uint32_t)(sizeof("[init] pty read master failed\n") - 1));
            sys_exit(1);
        }

        (void)sys_close(mfd);
        (void)sys_close(sfd);
        sys_write(1, "[init] pty OK\n", (uint32_t)(sizeof("[init] pty OK\n") - 1));
    }

    {
        sys_write(1, "[init] setsid test: before fork\n",
                  (uint32_t)(sizeof("[init] setsid test: before fork\n") - 1));
        int pid = sys_fork();
        if (pid < 0) {
            static const char smsg[] = "[init] fork failed\n";
            (void)sys_write(1, smsg, (uint32_t)(sizeof(smsg) - 1));
            sys_exit(2);
        }
        if (pid == 0) {
            sys_write(1, "[init] setsid test: child start\n",
                      (uint32_t)(sizeof("[init] setsid test: child start\n") - 1));
            int me = sys_getpid();
            int sid = sys_setsid();
            if (sid != me) sys_exit(2);

            int pg = sys_getpgrp();
            if (pg != me) sys_exit(3);

            int newpg = me + 1;
            if (sys_setpgid(0, newpg) < 0) sys_exit(4);
            if (sys_getpgrp() != newpg) sys_exit(5);

            sys_exit(0);
        }

        sys_write(1, "[init] setsid test: parent waitpid\n",
                  (uint32_t)(sizeof("[init] setsid test: parent waitpid\n") - 1));
        int st = 0;
        int wp = sys_waitpid(pid, &st, 0);
        if (wp != pid || st != 0) {
            sys_write(1, "[init] setsid/setpgid/getpgrp failed\n",
                      (uint32_t)(sizeof("[init] setsid/setpgid/getpgrp failed\n") - 1));
            sys_exit(1);
        }

        sys_write(1, "[init] setsid/setpgid/getpgrp OK\n",
                  (uint32_t)(sizeof("[init] setsid/setpgid/getpgrp OK\n") - 1));
    }

    {
        uintptr_t oldh = 0;
        if (sys_sigaction(SIGUSR1, usr1_handler, &oldh) < 0) {
            sys_write(1, "[init] sigaction failed\n",
                      (uint32_t)(sizeof("[init] sigaction failed\n") - 1));
            sys_exit(1);
        }

        int me = sys_getpid();
        if (sys_kill(me, SIGUSR1) < 0) {
            sys_write(1, "[init] kill(SIGUSR1) failed\n",
                      (uint32_t)(sizeof("[init] kill(SIGUSR1) failed\n") - 1));
            sys_exit(1);
        }

        for (uint32_t i = 0; i < 2000000U; i++) {
            if (got_usr1) break;
        }

        if (!got_usr1) {
            sys_write(1, "[init] SIGUSR1 not delivered\n",
                      (uint32_t)(sizeof("[init] SIGUSR1 not delivered\n") - 1));
            sys_exit(1);
        }

        sys_write(1, "[init] sigaction/kill(SIGUSR1) OK\n",
                  (uint32_t)(sizeof("[init] sigaction/kill(SIGUSR1) OK\n") - 1));
    }

    // Verify that returning from a signal handler does not corrupt the user stack.
    {
        if (sys_sigaction(SIGUSR1, usr1_ret_handler, 0) < 0) {
            sys_write(1, "[init] sigaction (sigreturn test) failed\n",
                      (uint32_t)(sizeof("[init] sigaction (sigreturn test) failed\n") - 1));
            sys_exit(1);
        }

        volatile uint32_t canary = 0x11223344U;
        int me = sys_getpid();
        if (sys_kill(me, SIGUSR1) < 0) {
            sys_write(1, "[init] kill(SIGUSR1) (sigreturn test) failed\n",
                      (uint32_t)(sizeof("[init] kill(SIGUSR1) (sigreturn test) failed\n") - 1));
            sys_exit(1);
        }

        if (!got_usr1_ret) {
            sys_write(1, "[init] SIGUSR1 not delivered (sigreturn test)\n",
                      (uint32_t)(sizeof("[init] SIGUSR1 not delivered (sigreturn test)\n") - 1));
            sys_exit(1);
        }

        if (canary != 0x11223344U) {
            sys_write(1, "[init] sigreturn test stack corruption\n",
                      (uint32_t)(sizeof("[init] sigreturn test stack corruption\n") - 1));
            sys_exit(1);
        }

        sys_write(1, "[init] sigreturn OK\n",
                  (uint32_t)(sizeof("[init] sigreturn OK\n") - 1));
    }

    fd = sys_open("/tmp/hello.txt", 0);
    if (fd < 0) {
        sys_write(1, "[init] tmpfs open2 failed\n",
                  (uint32_t)(sizeof("[init] tmpfs open2 failed\n") - 1));
        sys_exit(1);
    }

    if (sys_stat("/tmp/hello.txt", &st) < 0) {
        sys_write(1, "[init] tmpfs stat failed\n",
                  (uint32_t)(sizeof("[init] tmpfs stat failed\n") - 1));
        sys_exit(1);
    }
    if ((st.st_mode & S_IFMT) != S_IFREG) {
        sys_write(1, "[init] tmpfs stat not reg\n",
                  (uint32_t)(sizeof("[init] tmpfs stat not reg\n") - 1));
        sys_exit(1);
    }
    if (st.st_size == 0) {
        sys_write(1, "[init] tmpfs stat size 0\n",
                  (uint32_t)(sizeof("[init] tmpfs stat size 0\n") - 1));
        sys_exit(1);
    }

    struct stat fst;
    if (sys_fstat(fd, &fst) < 0) {
        sys_write(1, "[init] tmpfs fstat failed\n",
                  (uint32_t)(sizeof("[init] tmpfs fstat failed\n") - 1));
        sys_exit(1);
    }
    if (fst.st_size != st.st_size) {
        sys_write(1, "[init] tmpfs stat size mismatch\n",
                  (uint32_t)(sizeof("[init] tmpfs stat size mismatch\n") - 1));
        sys_exit(1);
    }

    int end = sys_lseek(fd, 0, SEEK_END);
    if (end < 0 || (uint32_t)end != st.st_size) {
        sys_write(1, "[init] tmpfs lseek end bad\n",
                  (uint32_t)(sizeof("[init] tmpfs lseek end bad\n") - 1));
        sys_exit(1);
    }

    uint8_t eofb;
    if (sys_read(fd, &eofb, 1) != 0) {
        sys_write(1, "[init] tmpfs eof read bad\n",
                  (uint32_t)(sizeof("[init] tmpfs eof read bad\n") - 1));
        sys_exit(1);
    }

    if (sys_lseek(fd, 0, 999) >= 0) {
        sys_write(1, "[init] tmpfs lseek whence bad\n",
                  (uint32_t)(sizeof("[init] tmpfs lseek whence bad\n") - 1));
        sys_exit(1);
    }

    if (sys_lseek(fd, 0, SEEK_SET) < 0) {
        sys_write(1, "[init] tmpfs lseek set failed\n",
                  (uint32_t)(sizeof("[init] tmpfs lseek set failed\n") - 1));
        sys_exit(1);
    }

    uint8_t tbuf[6];
    if (sys_read(fd, tbuf, 5) != 5) {
        sys_write(1, "[init] tmpfs read failed\n",
                  (uint32_t)(sizeof("[init] tmpfs read failed\n") - 1));
        sys_exit(1);
    }
    tbuf[5] = 0;
    if (tbuf[0] != 'h' || tbuf[1] != 'e' || tbuf[2] != 'l' || tbuf[3] != 'l' || tbuf[4] != 'o') {
        sys_write(1, "[init] tmpfs bad data\n", (uint32_t)(sizeof("[init] tmpfs bad data\n") - 1));
        sys_exit(1);
    }

    if (sys_close(fd) < 0) {
        sys_write(1, "[init] tmpfs close failed\n",
                  (uint32_t)(sizeof("[init] tmpfs close failed\n") - 1));
        sys_exit(1);
    }

    if (sys_open("/tmp/does_not_exist", 0) >= 0) {
        sys_write(1, "[init] tmpfs open nonexist bad\n",
                  (uint32_t)(sizeof("[init] tmpfs open nonexist bad\n") - 1));
        sys_exit(1);
    }

    fd = sys_open("/tmp/hello.txt", 0);
    if (fd < 0) {
        sys_write(1, "[init] tmpfs open3 failed\n",
                  (uint32_t)(sizeof("[init] tmpfs open3 failed\n") - 1));
        sys_exit(1);
    }

    if (sys_fstat(fd, &fst) < 0) {
        sys_write(1, "[init] tmpfs fstat2 failed\n",
                  (uint32_t)(sizeof("[init] tmpfs fstat2 failed\n") - 1));
        sys_exit(1);
    }

    if (sys_lseek(fd, 0, SEEK_END) < 0) {
        sys_write(1, "[init] tmpfs lseek end2 failed\n",
                  (uint32_t)(sizeof("[init] tmpfs lseek end2 failed\n") - 1));
        sys_exit(1);
    }

    char suf[3];
    suf[0] = 'X';
    suf[1] = 'Y';
    suf[2] = 'Z';
    if (sys_write(fd, suf, 3) != 3) {
        sys_write(1, "[init] tmpfs write failed\n",
                  (uint32_t)(sizeof("[init] tmpfs write failed\n") - 1));
        sys_exit(1);
    }

    if (sys_fstat(fd, &fst) < 0) {
        sys_write(1, "[init] tmpfs fstat3 failed\n",
                  (uint32_t)(sizeof("[init] tmpfs fstat3 failed\n") - 1));
        sys_exit(1);
    }
    if (fst.st_size != st.st_size + 3) {
        sys_write(1, "[init] tmpfs size not grown\n",
                  (uint32_t)(sizeof("[init] tmpfs size not grown\n") - 1));
        sys_exit(1);
    }

    if (sys_lseek(fd, -3, SEEK_END) < 0) {
        sys_write(1, "[init] tmpfs lseek back failed\n",
                  (uint32_t)(sizeof("[init] tmpfs lseek back failed\n") - 1));
        sys_exit(1);
    }
    uint8_t s2[3];
    if (sys_read(fd, s2, 3) != 3 || s2[0] != 'X' || s2[1] != 'Y' || s2[2] != 'Z') {
        sys_write(1, "[init] tmpfs suffix mismatch\n",
                  (uint32_t)(sizeof("[init] tmpfs suffix mismatch\n") - 1));
        sys_exit(1);
    }

    if (sys_close(fd) < 0) {
        sys_write(1, "[init] tmpfs close3 failed\n",
                  (uint32_t)(sizeof("[init] tmpfs close3 failed\n") - 1));
        sys_exit(1);
    }

    sys_write(1, "[init] tmpfs/mount OK\n", (uint32_t)(sizeof("[init] tmpfs/mount OK\n") - 1));

    {
        int fd = sys_open("/dev/null", 0);
        if (fd < 0) {
            sys_write(1, "[init] /dev/null open failed\n",
                      (uint32_t)(sizeof("[init] /dev/null open failed\n") - 1));
            sys_exit(1);
        }
        static const char z[] = "discard me";
        if (sys_write(fd, z, (uint32_t)(sizeof(z) - 1)) != (int)(sizeof(z) - 1)) {
            sys_write(1, "[init] /dev/null write failed\n",
                      (uint32_t)(sizeof("[init] /dev/null write failed\n") - 1));
            sys_exit(1);
        }
        (void)sys_close(fd);
        sys_write(1, "[init] /dev/null OK\n", (uint32_t)(sizeof("[init] /dev/null OK\n") - 1));
    }

    // B1: persistent storage smoke. Value should increment across reboots (disk.img).
    {
        int fd = sys_open("/persist/counter", 0);
        if (fd < 0) {
            sys_write(1, "[init] /persist/counter open failed\n",
                      (uint32_t)(sizeof("[init] /persist/counter open failed\n") - 1));
            sys_exit(1);
        }

        (void)sys_lseek(fd, 0, SEEK_SET);
        uint8_t b[4] = {0, 0, 0, 0};
        int rd = sys_read(fd, b, 4);
        if (rd != 4) {
            sys_write(1, "[init] /persist/counter read failed\n",
                      (uint32_t)(sizeof("[init] /persist/counter read failed\n") - 1));
            sys_exit(1);
        }

        uint32_t v = (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
        v++;
        b[0] = (uint8_t)(v & 0xFF);
        b[1] = (uint8_t)((v >> 8) & 0xFF);
        b[2] = (uint8_t)((v >> 16) & 0xFF);
        b[3] = (uint8_t)((v >> 24) & 0xFF);

        (void)sys_lseek(fd, 0, SEEK_SET);
        int wr = sys_write(fd, b, 4);
        if (wr != 4) {
            sys_write(1, "[init] /persist/counter write failed\n",
                      (uint32_t)(sizeof("[init] /persist/counter write failed\n") - 1));
            sys_exit(1);
        }

        (void)sys_close(fd);

        sys_write(1, "[init] /persist/counter=", (uint32_t)(sizeof("[init] /persist/counter=") - 1));
        write_int_dec((int)v);
        sys_write(1, "\n", 1);
    }

    {
        int fd = sys_open("/dev/tty", 0);
        if (fd < 0) {
            sys_write(1, "[init] /dev/tty open failed\n",
                      (uint32_t)(sizeof("[init] /dev/tty open failed\n") - 1));
            sys_exit(1);
        }
        static const char m[] = "[init] /dev/tty write OK\n";
        int wr = sys_write(fd, m, (uint32_t)(sizeof(m) - 1));
        if (wr != (int)(sizeof(m) - 1)) {
            sys_write(1, "[init] /dev/tty write failed\n",
                      (uint32_t)(sizeof("[init] /dev/tty write failed\n") - 1));
            sys_exit(1);
        }
        (void)sys_close(fd);
    }

    // B2: on-disk general filesystem smoke (/disk)
    {
        int fd = sys_open("/disk/test", O_CREAT);
        if (fd < 0) {
            sys_write(1, "[init] /disk/test open failed\n",
                      (uint32_t)(sizeof("[init] /disk/test open failed\n") - 1));
            sys_exit(1);
        }

        char buf[16];
        int rd = sys_read(fd, buf, sizeof(buf));
        int prev = 0;
        if (rd > 0) {
            for (int i = 0; i < rd; i++) {
                if (buf[i] < '0' || buf[i] > '9') break;
                prev = prev * 10 + (buf[i] - '0');
            }
        }

        (void)sys_close(fd);

        fd = sys_open("/disk/test", O_CREAT | O_TRUNC);
        if (fd < 0) {
            sys_write(1, "[init] /disk/test open2 failed\n",
                      (uint32_t)(sizeof("[init] /disk/test open2 failed\n") - 1));
            sys_exit(1);
        }

        int next = prev + 1;
        char out[16];
        int n = 0;
        int v = next;
        if (v == 0) {
            out[n++] = '0';
        } else {
            char tmp[16];
            int t = 0;
            while (v > 0 && t < (int)sizeof(tmp)) {
                tmp[t++] = (char)('0' + (v % 10));
                v /= 10;
            }
            while (t > 0) {
                out[n++] = tmp[--t];
            }
        }

        if (sys_write(fd, out, (uint32_t)n) != n) {
            sys_write(1, "[init] /disk/test write failed\n",
                      (uint32_t)(sizeof("[init] /disk/test write failed\n") - 1));
            sys_exit(1);
        }
        (void)sys_close(fd);

        fd = sys_open("/disk/test", 0);
        if (fd < 0) {
            sys_write(1, "[init] /disk/test open3 failed\n",
                      (uint32_t)(sizeof("[init] /disk/test open3 failed\n") - 1));
            sys_exit(1);
        }
        for (uint32_t i = 0; i < (uint32_t)sizeof(buf); i++) buf[i] = 0;
        rd = sys_read(fd, buf, sizeof(buf));
        (void)sys_close(fd);
        if (rd != n || !memeq(buf, out, (uint32_t)n)) {
            sys_write(1, "[init] /disk/test verify failed\n",
                      (uint32_t)(sizeof("[init] /disk/test verify failed\n") - 1));
            sys_exit(1);
        }

        sys_write(1, "[init] /disk/test prev=", (uint32_t)(sizeof("[init] /disk/test prev=") - 1));
        write_int_dec(prev);
        sys_write(1, " next=", (uint32_t)(sizeof(" next=") - 1));
        write_int_dec(next);
        sys_write(1, " OK\n", (uint32_t)(sizeof(" OK\n") - 1));
    }

    // B3: diskfs mkdir/unlink smoke
    {
        int r = sys_mkdir("/disk/dir");
        if (r < 0 && errno != 17) {
            sys_write(1, "[init] mkdir /disk/dir failed errno=", (uint32_t)(sizeof("[init] mkdir /disk/dir failed errno=") - 1));
            write_int_dec(errno);
            sys_write(1, "\n", 1);
            sys_exit(1);
        }

        int fd = sys_open("/disk/dir/file", O_CREAT | O_TRUNC);
        if (fd < 0) {
            sys_write(1, "[init] open /disk/dir/file failed\n",
                      (uint32_t)(sizeof("[init] open /disk/dir/file failed\n") - 1));
            sys_exit(1);
        }
        static const char msg2[] = "ok";
        if (sys_write(fd, msg2, 2) != 2) {
            sys_write(1, "[init] write /disk/dir/file failed\n",
                      (uint32_t)(sizeof("[init] write /disk/dir/file failed\n") - 1));
            sys_exit(1);
        }
        (void)sys_close(fd);

        r = sys_unlink("/disk/dir/file");
        if (r < 0) {
            sys_write(1, "[init] unlink /disk/dir/file failed\n",
                      (uint32_t)(sizeof("[init] unlink /disk/dir/file failed\n") - 1));
            sys_exit(1);
        }

        fd = sys_open("/disk/dir/file", 0);
        if (fd >= 0) {
            sys_write(1, "[init] unlink did not remove file\n",
                      (uint32_t)(sizeof("[init] unlink did not remove file\n") - 1));
            sys_exit(1);
        }

        sys_write(1, "[init] diskfs mkdir/unlink OK\n",
                  (uint32_t)(sizeof("[init] diskfs mkdir/unlink OK\n") - 1));
    }

    // B4: diskfs getdents smoke
    {
        int r = sys_mkdir("/disk/ls");
        if (r < 0 && errno != 17) {
            sys_write(1, "[init] mkdir /disk/ls failed errno=", (uint32_t)(sizeof("[init] mkdir /disk/ls failed errno=") - 1));
            write_int_dec(errno);
            sys_write(1, "\n", 1);
            sys_exit(1);
        }

        int fd = sys_open("/disk/ls/file1", O_CREAT | O_TRUNC);
        if (fd < 0) {
            sys_write(1, "[init] create /disk/ls/file1 failed\n",
                      (uint32_t)(sizeof("[init] create /disk/ls/file1 failed\n") - 1));
            sys_exit(1);
        }
        (void)sys_close(fd);

        fd = sys_open("/disk/ls/file2", O_CREAT | O_TRUNC);
        if (fd < 0) {
            sys_write(1, "[init] create /disk/ls/file2 failed\n",
                      (uint32_t)(sizeof("[init] create /disk/ls/file2 failed\n") - 1));
            sys_exit(1);
        }
        (void)sys_close(fd);

        int dfd = sys_open("/disk/ls", 0);
        if (dfd < 0) {
            sys_write(1, "[init] open dir /disk/ls failed\n",
                      (uint32_t)(sizeof("[init] open dir /disk/ls failed\n") - 1));
            sys_exit(1);
        }

        struct {
            uint32_t d_ino;
            uint16_t d_reclen;
            uint8_t d_type;
            char d_name[24];
        } ents[8];

        int n = sys_getdents(dfd, ents, (uint32_t)sizeof(ents));
        (void)sys_close(dfd);
        if (n <= 0) {
            sys_write(1, "[init] getdents failed\n",
                      (uint32_t)(sizeof("[init] getdents failed\n") - 1));
            sys_exit(1);
        }

        int saw_dot = 0, saw_dotdot = 0, saw_f1 = 0, saw_f2 = 0;
        int cnt = n / (int)sizeof(ents[0]);
        for (int i = 0; i < cnt; i++) {
            if (streq(ents[i].d_name, ".")) saw_dot = 1;
            else if (streq(ents[i].d_name, "..")) saw_dotdot = 1;
            else if (streq(ents[i].d_name, "file1")) saw_f1 = 1;
            else if (streq(ents[i].d_name, "file2")) saw_f2 = 1;
        }

        if (!saw_dot || !saw_dotdot || !saw_f1 || !saw_f2) {
            sys_write(1, "[init] getdents verify failed\n",
                      (uint32_t)(sizeof("[init] getdents verify failed\n") - 1));
            sys_exit(1);
        }

        sys_write(1, "[init] diskfs getdents OK\n",
                  (uint32_t)(sizeof("[init] diskfs getdents OK\n") - 1));
    }

    // B5: isatty() POSIX-like smoke (via ioctl TCGETS)
    {
        int fd = sys_open("/dev/tty", 0);
        if (fd < 0) {
            sys_write(1, "[init] isatty open /dev/tty failed\n",
                      (uint32_t)(sizeof("[init] isatty open /dev/tty failed\n") - 1));
            sys_exit(1);
        }
        int r = isatty_fd(fd);
        (void)sys_close(fd);
        if (r != 1) {
            sys_write(1, "[init] isatty(/dev/tty) failed\n",
                      (uint32_t)(sizeof("[init] isatty(/dev/tty) failed\n") - 1));
            sys_exit(1);
        }

        fd = sys_open("/dev/null", 0);
        if (fd < 0) {
            sys_write(1, "[init] isatty open /dev/null failed\n",
                      (uint32_t)(sizeof("[init] isatty open /dev/null failed\n") - 1));
            sys_exit(1);
        }
        r = isatty_fd(fd);
        (void)sys_close(fd);
        if (r != 0) {
            sys_write(1, "[init] isatty(/dev/null) expected 0\n",
                      (uint32_t)(sizeof("[init] isatty(/dev/null) expected 0\n") - 1));
            sys_exit(1);
        }

        sys_write(1, "[init] isatty OK\n", (uint32_t)(sizeof("[init] isatty OK\n") - 1));
    }

    // B6: O_NONBLOCK smoke (pipe + pty)
    {
        int fds[2];
        if (sys_pipe(fds) < 0) {
            sys_write(1, "[init] pipe for nonblock failed\n",
                      (uint32_t)(sizeof("[init] pipe for nonblock failed\n") - 1));
            sys_exit(1);
        }

        if (sys_fcntl(fds[0], F_SETFL, O_NONBLOCK) < 0) {
            sys_write(1, "[init] fcntl nonblock pipe failed\n",
                      (uint32_t)(sizeof("[init] fcntl nonblock pipe failed\n") - 1));
            sys_exit(1);
        }

        char b;
        int r = sys_read(fds[0], &b, 1);
        if (r != -1 || errno != EAGAIN) {
            sys_write(1, "[init] nonblock pipe read expected EAGAIN\n",
                      (uint32_t)(sizeof("[init] nonblock pipe read expected EAGAIN\n") - 1));
            sys_exit(1);
        }

        if (sys_write(fds[1], "x", 1) != 1) {
            sys_write(1, "[init] pipe write failed\n",
                      (uint32_t)(sizeof("[init] pipe write failed\n") - 1));
            sys_exit(1);
        }
        r = sys_read(fds[0], &b, 1);
        if (r != 1 || b != 'x') {
            sys_write(1, "[init] nonblock pipe read after write failed\n",
                      (uint32_t)(sizeof("[init] nonblock pipe read after write failed\n") - 1));
            sys_exit(1);
        }

        (void)sys_close(fds[0]);
        (void)sys_close(fds[1]);

        int p = sys_open("/dev/ptmx", 0);
        if (p < 0) {
            sys_write(1, "[init] open /dev/ptmx failed\n",
                      (uint32_t)(sizeof("[init] open /dev/ptmx failed\n") - 1));
            sys_exit(1);
        }
        if (sys_fcntl(p, F_SETFL, O_NONBLOCK) < 0) {
            sys_write(1, "[init] fcntl nonblock ptmx failed\n",
                      (uint32_t)(sizeof("[init] fcntl nonblock ptmx failed\n") - 1));
            sys_exit(1);
        }
        char pch;
        r = sys_read(p, &pch, 1);
        if (r != -1 || errno != EAGAIN) {
            sys_write(1, "[init] nonblock ptmx read expected EAGAIN\n",
                      (uint32_t)(sizeof("[init] nonblock ptmx read expected EAGAIN\n") - 1));
            sys_exit(1);
        }
        (void)sys_close(p);

        sys_write(1, "[init] O_NONBLOCK OK\n",
                  (uint32_t)(sizeof("[init] O_NONBLOCK OK\n") - 1));
    }

    // B6b: pipe2 + dup3 smoke
    {
        int fds[2];
        if (sys_pipe2(fds, O_NONBLOCK) < 0) {
            sys_write(1, "[init] pipe2 failed\n",
                      (uint32_t)(sizeof("[init] pipe2 failed\n") - 1));
            sys_exit(1);
        }

        char b;
        int r = sys_read(fds[0], &b, 1);
        if (r != -1 || errno != EAGAIN) {
            sys_write(1, "[init] pipe2 nonblock read expected EAGAIN\n",
                      (uint32_t)(sizeof("[init] pipe2 nonblock read expected EAGAIN\n") - 1));
            sys_exit(1);
        }

        int d = sys_dup3(fds[0], fds[0], 0);
        if (d != -1 || errno != EINVAL) {
            sys_write(1, "[init] dup3 samefd expected EINVAL\n",
                      (uint32_t)(sizeof("[init] dup3 samefd expected EINVAL\n") - 1));
            sys_exit(1);
        }

        (void)sys_close(fds[0]);
        (void)sys_close(fds[1]);
        sys_write(1, "[init] pipe2/dup3 OK\n",
                  (uint32_t)(sizeof("[init] pipe2/dup3 OK\n") - 1));
    }

    // B7: chdir/getcwd smoke + relative paths
    {
        int r = sys_mkdir("/disk/cwd");
        if (r < 0 && errno != 17) {
            sys_write(1, "[init] mkdir /disk/cwd failed\n",
                      (uint32_t)(sizeof("[init] mkdir /disk/cwd failed\n") - 1));
            sys_exit(1);
        }

        r = sys_chdir("/disk/cwd");
        if (r < 0) {
            sys_write(1, "[init] chdir failed\n",
                      (uint32_t)(sizeof("[init] chdir failed\n") - 1));
            sys_exit(1);
        }

        char cwd[64];
        for (uint32_t i = 0; i < (uint32_t)sizeof(cwd); i++) cwd[i] = 0;
        if (sys_getcwd(cwd, (uint32_t)sizeof(cwd)) < 0) {
            sys_write(1, "[init] getcwd failed\n",
                      (uint32_t)(sizeof("[init] getcwd failed\n") - 1));
            sys_exit(1);
        }

        // Create file using relative path.
        int fd = sys_open("rel", O_CREAT | O_TRUNC);
        if (fd < 0) {
            sys_write(1, "[init] open relative failed\n",
                      (uint32_t)(sizeof("[init] open relative failed\n") - 1));
            sys_exit(1);
        }
        (void)sys_close(fd);

        // Stat with relative path.
        struct stat st;
        if (sys_stat("rel", &st) < 0) {
            sys_write(1, "[init] stat relative failed\n",
                      (uint32_t)(sizeof("[init] stat relative failed\n") - 1));
            sys_exit(1);
        }

        sys_write(1, "[init] chdir/getcwd OK\n",
                  (uint32_t)(sizeof("[init] chdir/getcwd OK\n") - 1));
    }

    // B8: *at() syscalls smoke (AT_FDCWD)
    {
        int fd = sys_openat(AT_FDCWD, "atfile", O_CREAT | O_TRUNC, 0);
        if (fd < 0) {
            sys_write(1, "[init] openat failed\n",
                      (uint32_t)(sizeof("[init] openat failed\n") - 1));
            sys_exit(1);
        }
        (void)sys_close(fd);

        struct stat st;
        if (sys_fstatat(AT_FDCWD, "atfile", &st, 0) < 0) {
            sys_write(1, "[init] fstatat failed\n",
                      (uint32_t)(sizeof("[init] fstatat failed\n") - 1));
            sys_exit(1);
        }

        if (sys_unlinkat(AT_FDCWD, "atfile", 0) < 0) {
            sys_write(1, "[init] unlinkat failed\n",
                      (uint32_t)(sizeof("[init] unlinkat failed\n") - 1));
            sys_exit(1);
        }

        if (sys_stat("atfile", &st) >= 0) {
            sys_write(1, "[init] unlinkat did not remove file\n",
                      (uint32_t)(sizeof("[init] unlinkat did not remove file\n") - 1));
            sys_exit(1);
        }

        sys_write(1, "[init] *at OK\n",
                  (uint32_t)(sizeof("[init] *at OK\n") - 1));
    }

    // B9: rename + rmdir smoke
    {
        // Create a file, rename it, verify old gone and new exists.
        int fd = sys_open("/disk/rnold", O_CREAT | O_TRUNC);
        if (fd < 0) {
            sys_write(1, "[init] rename: create failed\n",
                      (uint32_t)(sizeof("[init] rename: create failed\n") - 1));
            sys_exit(1);
        }
        (void)sys_write(fd, "RN", 2);
        (void)sys_close(fd);

        if (sys_rename("/disk/rnold", "/disk/rnnew") < 0) {
            sys_write(1, "[init] rename failed\n",
                      (uint32_t)(sizeof("[init] rename failed\n") - 1));
            sys_exit(1);
        }

        struct stat st;
        if (sys_stat("/disk/rnold", &st) >= 0) {
            sys_write(1, "[init] rename: old still exists\n",
                      (uint32_t)(sizeof("[init] rename: old still exists\n") - 1));
            sys_exit(1);
        }
        if (sys_stat("/disk/rnnew", &st) < 0) {
            sys_write(1, "[init] rename: new not found\n",
                      (uint32_t)(sizeof("[init] rename: new not found\n") - 1));
            sys_exit(1);
        }

        (void)sys_unlink("/disk/rnnew");

        // mkdir, then rmdir
        if (sys_mkdir("/disk/rmtmp") < 0 && errno != 17) {
            sys_write(1, "[init] rmdir: mkdir failed\n",
                      (uint32_t)(sizeof("[init] rmdir: mkdir failed\n") - 1));
            sys_exit(1);
        }
        if (sys_rmdir("/disk/rmtmp") < 0) {
            sys_write(1, "[init] rmdir failed\n",
                      (uint32_t)(sizeof("[init] rmdir failed\n") - 1));
            sys_exit(1);
        }
        if (sys_stat("/disk/rmtmp", &st) >= 0) {
            sys_write(1, "[init] rmdir: dir still exists\n",
                      (uint32_t)(sizeof("[init] rmdir: dir still exists\n") - 1));
            sys_exit(1);
        }

        sys_write(1, "[init] rename/rmdir OK\n",
                  (uint32_t)(sizeof("[init] rename/rmdir OK\n") - 1));
    }

    // B10: getdents on /dev (devfs) and /tmp (tmpfs)
    {
        int devfd = sys_open("/dev", 0);
        if (devfd < 0) {
            sys_write(1, "[init] open /dev failed\n",
                      (uint32_t)(sizeof("[init] open /dev failed\n") - 1));
            sys_exit(1);
        }
        char dbuf[256];
        int dr = sys_getdents(devfd, dbuf, (uint32_t)sizeof(dbuf));
        (void)sys_close(devfd);
        if (dr <= 0) {
            sys_write(1, "[init] getdents /dev failed\n",
                      (uint32_t)(sizeof("[init] getdents /dev failed\n") - 1));
            sys_exit(1);
        }

        int tmpfd = sys_open("/tmp", 0);
        if (tmpfd < 0) {
            sys_write(1, "[init] open /tmp failed\n",
                      (uint32_t)(sizeof("[init] open /tmp failed\n") - 1));
            sys_exit(1);
        }
        char tbuf[256];
        int tr = sys_getdents(tmpfd, tbuf, (uint32_t)sizeof(tbuf));
        (void)sys_close(tmpfd);
        if (tr <= 0) {
            sys_write(1, "[init] getdents /tmp failed\n",
                      (uint32_t)(sizeof("[init] getdents /tmp failed\n") - 1));
            sys_exit(1);
        }

        sys_write(1, "[init] getdents multi-fs OK\n",
                  (uint32_t)(sizeof("[init] getdents multi-fs OK\n") - 1));
    }

    enum { NCHILD = 100 };
    int children[NCHILD];
    for (int i = 0; i < NCHILD; i++) {
        int pid = sys_fork();
        if (pid < 0) {
            static const char smsg[] = "[init] fork failed\n";
            (void)sys_write(1, smsg, (uint32_t)(sizeof(smsg) - 1));
            sys_exit(2);
        }
        if (pid == 0) {
            sys_exit(42);
        }
        children[i] = pid;
    }

    {
        int parent_pid = sys_getpid();
        int pid = sys_fork();
        if (pid == 0) {
            int ppid = sys_getppid();
            if (ppid == parent_pid) {
                static const char msg[] = "[init] getppid OK\n";
                (void)sys_write(1, msg, (uint32_t)(sizeof(msg) - 1));
                sys_exit(0);
            }
            static const char msg[] = "[init] getppid failed\n";
            (void)sys_write(1, msg, (uint32_t)(sizeof(msg) - 1));
            sys_exit(1);
        }
        int st = 0;
        (void)sys_waitpid(pid, &st, 0);
    }

    {
        int pid = sys_fork();
        if (pid == 0) {
            volatile uint32_t x = 0;
            for (uint32_t i = 0; i < 2000000U; i++) x += i;
            sys_exit(7);
        }
        int st = 0;
        int wp = sys_waitpid(pid, &st, WNOHANG);
        if (wp == 0 || wp == pid) {
            static const char msg[] = "[init] waitpid WNOHANG OK\n";
            (void)sys_write(1, msg, (uint32_t)(sizeof(msg) - 1));
        } else {
            static const char msg[] = "[init] waitpid WNOHANG failed\n";
            (void)sys_write(1, msg, (uint32_t)(sizeof(msg) - 1));
        }
        if (wp == 0) {
            (void)sys_waitpid(pid, &st, 0);
        }
    }

    {
        int pid = sys_fork();
        if (pid < 0) {
            static const char msg[] = "[init] sigsegv test fork failed\n";
            (void)sys_write(1, msg, (uint32_t)(sizeof(msg) - 1));
            sys_exit(1);
        }

        if (pid == 0) {
            struct sigaction act;
            act.sa_handler = 0;
            act.sa_sigaction = (uintptr_t)sigsegv_info_handler;
            act.sa_mask = 0;
            act.sa_flags = SA_SIGINFO;

            if (sys_sigaction2(SIGSEGV, &act, 0) < 0) {
                static const char msg[] = "[init] sigaction(SIGSEGV) failed\n";
                (void)sys_write(1, msg, (uint32_t)(sizeof(msg) - 1));
                sys_exit(1);
            }

            *(volatile uint32_t*)0x12345000U = 123;
            sys_exit(2);
        }

        int st = 0;
        int wp = sys_waitpid(pid, &st, 0);
        if (wp == pid && st == 0) {
            static const char msg[] = "[init] SIGSEGV OK\n";
            (void)sys_write(1, msg, (uint32_t)(sizeof(msg) - 1));
        } else {
            static const char msg[] = "[init] SIGSEGV failed\n";
            (void)sys_write(1, msg, (uint32_t)(sizeof(msg) - 1));
            sys_exit(1);
        }
    }

    int ok = 1;
    for (int i = 0; i < NCHILD; i++) {
        int st = 0;
        int wp = sys_waitpid(children[i], &st, 0);
        if (wp != children[i] || st != 42) {
            ok = 0;
            break;
        }
    }

    if (ok) {
        static const char wmsg[] = "[init] waitpid OK (100 children, explicit)\n";
        (void)sys_write(1, wmsg, (uint32_t)(sizeof(wmsg) - 1));
    } else {
        static const char wbad[] = "[init] waitpid failed (100 children, explicit)\n";
        (void)sys_write(1, wbad, (uint32_t)(sizeof(wbad) - 1));
    }

    (void)sys_write(1, "[init] execve(/bin/echo.elf)\n",
                    (uint32_t)(sizeof("[init] execve(/bin/echo.elf)\n") - 1));
    static const char* const argv[] = {"echo.elf", "arg1", "arg2", 0};
    static const char* const envp[] = {"FOO=bar", "HELLO=world", 0};
    (void)sys_execve("/bin/echo.elf", argv, envp);
    (void)sys_write(1, "[init] execve returned (unexpected)\n",
                    (uint32_t)(sizeof("[init] execve returned (unexpected)\n") - 1));
    sys_exit(1);
    sys_exit(0);
}
