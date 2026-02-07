#include <stdint.h>

enum {
    SYSCALL_WRITE = 1,
    SYSCALL_EXIT  = 2,
    SYSCALL_OPEN  = 4,
    SYSCALL_READ  = 5,
    SYSCALL_CLOSE = 6,
    SYSCALL_WAITPID = 7,
    SYSCALL_SPAWN = 8,
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
        static const char emsg[] = "[init] open(/bin/init.elf) failed\n";
        (void)sys_write(1, emsg, (uint32_t)(sizeof(emsg) - 1));
        sys_exit(1);
    }

    uint8_t hdr[4];
    int rd = sys_read(fd, hdr, (uint32_t)sizeof(hdr));
    (void)sys_close(fd);

    if (rd == 4 && hdr[0] == 0x7F && hdr[1] == 'E' && hdr[2] == 'L' && hdr[3] == 'F') {
        static const char ok[] = "[init] open/read/close OK (ELF magic)\n";
        (void)sys_write(1, ok, (uint32_t)(sizeof(ok) - 1));
    } else {
        static const char bad[] = "[init] read failed or bad header\n";
        (void)sys_write(1, bad, (uint32_t)(sizeof(bad) - 1));
    }

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
