#include <stdint.h>

enum {
    SYSCALL_WRITE = 1,
    SYSCALL_EXIT  = 2,
    SYSCALL_OPEN  = 4,
    SYSCALL_READ  = 5,
    SYSCALL_CLOSE = 6,
    SYSCALL_WAITPID = 7,
    SYSCALL_SPAWN = 8,
    SYSCALL_LSEEK = 9,
    SYSCALL_FSTAT = 10,
    SYSCALL_STAT = 11,
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

static int sys_spawn(void) {
    int ret;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_SPAWN)
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
        sys_write(1, "[init] open failed\n", 18);
        sys_exit(1);
    }

    uint8_t hdr[4];
    int rd = sys_read(fd, hdr, (uint32_t)sizeof(hdr));
    if (sys_close(fd) < 0) {
        sys_write(1, "[init] close failed\n", 19);
        sys_exit(1);
    }

    if (rd == 4 && hdr[0] == 0x7F && hdr[1] == 'E' && hdr[2] == 'L' && hdr[3] == 'F') {
        sys_write(1, "[init] open/read/close OK (ELF magic)\n",
                  (uint32_t)(sizeof("[init] open/read/close OK (ELF magic)\n") - 1));
    } else {
        sys_write(1, "[init] read failed or bad header\n", 30);
        sys_exit(1);
    }

    fd = sys_open("/bin/init.elf", 0);
    if (fd < 0) {
        sys_write(1, "[init] open2 failed\n", 19);
        sys_exit(1);
    }

    struct stat st;
    if (sys_fstat(fd, &st) < 0) {
        sys_write(1, "[init] fstat failed\n", 19);
        sys_exit(1);
    }

    if ((st.st_mode & S_IFMT) != S_IFREG || st.st_size == 0) {
        sys_write(1, "[init] fstat bad\n", 16);
        sys_exit(1);
    }

    if (sys_lseek(fd, 0, SEEK_SET) < 0) {
        sys_write(1, "[init] lseek set failed\n", 24);
        sys_exit(1);
    }

    uint8_t m2[4];
    if (sys_read(fd, m2, 4) != 4) {
        sys_write(1, "[init] read2 failed\n", 19);
        sys_exit(1);
    }
    if (m2[0] != 0x7F || m2[1] != 'E' || m2[2] != 'L' || m2[3] != 'F') {
        sys_write(1, "[init] lseek/read mismatch\n", 27);
        sys_exit(1);
    }

    if (sys_close(fd) < 0) {
        sys_write(1, "[init] close2 failed\n", 20);
        sys_exit(1);
    }

    if (sys_stat("/bin/init.elf", &st) < 0) {
        sys_write(1, "[init] stat failed\n", 18);
        sys_exit(1);
    }
    if ((st.st_mode & S_IFMT) != S_IFREG || st.st_size == 0) {
        sys_write(1, "[init] stat bad\n", 15);
        sys_exit(1);
    }

    sys_write(1, "[init] lseek/stat/fstat OK\n", 27);

    enum { NCHILD = 100 };
    int children[NCHILD];
    for (int i = 0; i < NCHILD; i++) {
        children[i] = sys_spawn();
        if (children[i] < 0) {
            static const char smsg[] = "[init] spawn failed\n";
            (void)sys_write(1, smsg, (uint32_t)(sizeof(smsg) - 1));
            sys_exit(2);
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
    sys_exit(0);
}
