#include <stdint.h>

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
};

enum {
    TCGETS = 0x5401,
    TCSETS = 0x5402,
    TIOCGPGRP = 0x540F,
    TIOCSPGRP = 0x5410,
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
    SIGSEGV = 11,
};

enum {
    WNOHANG = 1,
};

enum {
    SEEK_SET = 0,
    SEEK_CUR = 1,
    SEEK_END = 2,
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
    return ret;
}

static int sys_select(uint32_t nfds, uint64_t* readfds, uint64_t* writefds, uint64_t* exceptfds, int32_t timeout) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_SELECT), "b"(nfds), "c"(readfds), "d"(writefds), "S"(exceptfds), "D"(timeout)
        : "memory"
    );
    return ret;
}

static int sys_ioctl(int fd, uint32_t cmd, void* arg) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_IOCTL), "b"(fd), "c"(cmd), "d"(arg)
        : "memory"
    );
    return ret;
}

static int sys_kill(int pid, int sig) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_KILL), "b"(pid), "c"(sig)
        : "memory"
    );
    return ret;
}

static int sys_poll(struct pollfd* fds, uint32_t nfds, int32_t timeout) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_POLL), "b"(fds), "c"(nfds), "d"(timeout)
        : "memory"
    );
    return ret;
}

static int sys_setsid(void) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_SETSID)
        : "memory"
    );
    return ret;
}

static int sys_setpgid(int pid, int pgid) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_SETPGID), "b"(pid), "c"(pgid)
        : "memory"
    );
    return ret;
}

static int sys_getpgrp(void) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_GETPGRP)
        : "memory"
    );
    return ret;
}

static int sys_getpid(void) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_GETPID)
        : "memory"
    );
    return ret;
}

static int sys_getppid(void) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_GETPPID)
        : "memory"
    );
    return ret;
}

static int sys_fork(void) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_FORK)
        : "memory"
    );
    return ret;
}

static int sys_execve(const char* path, const char* const* argv, const char* const* envp) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_EXECVE), "b"(path), "c"(argv), "d"(envp)
        : "memory"
    );
    return ret;
}

static int sys_pipe(int fds[2]) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_PIPE), "b"(fds)
        : "memory"
    );
    return ret;
}

static int sys_dup(int oldfd) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_DUP), "b"(oldfd)
        : "memory"
    );
    return ret;
}

static int sys_dup2(int oldfd, int newfd) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_DUP2), "b"(oldfd), "c"(newfd)
        : "memory"
    );
    return ret;
}

static int sys_waitpid(int pid, int* status, uint32_t options) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_WAITPID), "b"(pid), "c"(status), "d"(options)
        : "memory"
    );
    return ret;
}

static int sys_open(const char* path, uint32_t flags) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_OPEN), "b"(path), "c"(flags)
        : "memory"
    );
    return ret;
}

static int sys_read(int fd, void* buf, uint32_t len) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_READ), "b"(fd), "c"(buf), "d"(len)
        : "memory"
    );
    return ret;
}

static int sys_close(int fd) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_CLOSE), "b"(fd)
        : "memory"
    );
    return ret;
}

static int sys_lseek(int fd, int32_t offset, int whence) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_LSEEK), "b"(fd), "c"(offset), "d"(whence)
        : "memory"
    );
    return ret;
}

static int sys_fstat(int fd, struct stat* st) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_FSTAT), "b"(fd), "c"(st)
        : "memory"
    );
    return ret;
}

static int sys_stat(const char* path, struct stat* st) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_STAT), "b"(path), "c"(st)
        : "memory"
    );
    return ret;
}

__attribute__((noreturn)) static void sys_exit(int code) {
    __asm__ volatile(
        "int $0x80\n"
        "1: jmp 1b\n"
        :
        : "a"(SYSCALL_EXIT), "b"(code)
        : "memory"
    );
    __builtin_unreachable();
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
        sys_write(1, "[init] open failed\n", (uint32_t)(sizeof("[init] open failed\n") - 1));
        sys_exit(1);
    }

    uint8_t hdr[4];
    int rd = sys_read(fd, hdr, (uint32_t)sizeof(hdr));
    if (sys_close(fd) < 0) {
        sys_write(1, "[init] close failed\n", (uint32_t)(sizeof("[init] close failed\n") - 1));
        sys_exit(1);
    }

    if (rd == 4 && hdr[0] == 0x7F && hdr[1] == 'E' && hdr[2] == 'L' && hdr[3] == 'F') {
        sys_write(1, "[init] open/read/close OK (ELF magic)\n",
                  (uint32_t)(sizeof("[init] open/read/close OK (ELF magic)\n") - 1));
    } else {
        sys_write(1, "[init] read failed or bad header\n",
                  (uint32_t)(sizeof("[init] read failed or bad header\n") - 1));
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
        sys_write(1, "[init] setsid test: before fork\n",
                  (uint32_t)(sizeof("[init] setsid test: before fork\n") - 1));
        int pid = sys_fork();
        if (pid < 0) {
            sys_write(1, "[init] setsid test fork failed\n",
                      (uint32_t)(sizeof("[init] setsid test fork failed\n") - 1));
            sys_exit(1);
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
        int wr = sys_write(fd, z, (uint32_t)(sizeof(z) - 1));
        if (wr != (int)(sizeof(z) - 1)) {
            sys_write(1, "[init] /dev/null write failed\n",
                      (uint32_t)(sizeof("[init] /dev/null write failed\n") - 1));
            sys_exit(1);
        }
        (void)sys_close(fd);
        sys_write(1, "[init] /dev/null OK\n", (uint32_t)(sizeof("[init] /dev/null OK\n") - 1));
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
            volatile uint32_t* p = (volatile uint32_t*)0;
            *p = 1;
            sys_exit(1);
        }

        int st = 0;
        int wp = sys_waitpid(pid, &st, 0);
        if (wp == pid && st == (128 + SIGSEGV)) {
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
