/* AdrOS ls utility */
#include <stdint.h>
#include "user_errno.h"

enum {
    SYSCALL_WRITE    = 1,
    SYSCALL_EXIT     = 2,
    SYSCALL_OPEN     = 4,
    SYSCALL_CLOSE    = 6,
    SYSCALL_GETDENTS = 30,
};

static int sys_write(int fd, const void* buf, uint32_t len) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret)
        : "a"(SYSCALL_WRITE), "b"(fd), "c"(buf), "d"(len) : "memory");
    return __syscall_fix(ret);
}

static int sys_open(const char* path, int flags) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret)
        : "a"(SYSCALL_OPEN), "b"(path), "c"(flags) : "memory");
    return __syscall_fix(ret);
}

static int sys_close(int fd) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret)
        : "a"(SYSCALL_CLOSE), "b"(fd) : "memory");
    return __syscall_fix(ret);
}

static int sys_getdents(int fd, void* buf, uint32_t len) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret)
        : "a"(SYSCALL_GETDENTS), "b"(fd), "c"(buf), "d"(len) : "memory");
    return __syscall_fix(ret);
}

static __attribute__((noreturn)) void sys_exit(int code) {
    __asm__ volatile("int $0x80" : : "a"(SYSCALL_EXIT), "b"(code) : "memory");
    for (;;) __asm__ volatile("hlt");
}

static uint32_t slen(const char* s) {
    uint32_t n = 0;
    while (s && s[n]) n++;
    return n;
}

static void wr(int fd, const char* s) {
    (void)sys_write(fd, s, slen(s));
}

struct dirent {
    uint32_t d_ino;
    uint16_t d_reclen;
    uint8_t  d_type;
    char     d_name[24];
};

static void ls_dir(const char* path) {
    int fd = sys_open(path, 0);
    if (fd < 0) {
        wr(2, "ls: cannot open ");
        wr(2, path);
        wr(2, "\n");
        return;
    }

    char buf[512];
    int rc = sys_getdents(fd, buf, sizeof(buf));
    if (rc > 0) {
        uint32_t off = 0;
        while (off + sizeof(struct dirent) <= (uint32_t)rc) {
            struct dirent* d = (struct dirent*)(buf + off);
            if (d->d_reclen == 0) break;
            /* skip . and .. */
            if (d->d_name[0] == '.' &&
                (d->d_name[1] == 0 ||
                 (d->d_name[1] == '.' && d->d_name[2] == 0))) {
                off += d->d_reclen;
                continue;
            }
            wr(1, d->d_name);
            wr(1, "\n");
            off += d->d_reclen;
        }
    }

    sys_close(fd);
}

static void ls_main(uint32_t* sp0) {
    uint32_t argc = sp0 ? sp0[0] : 0;
    const char* const* argv = (const char* const*)(sp0 + 1);

    if (argc <= 1) {
        ls_dir(".");
    } else {
        for (uint32_t i = 1; i < argc; i++) {
            if (argc > 2) {
                wr(1, argv[i]);
                wr(1, ":\n");
            }
            ls_dir(argv[i]);
        }
    }
    sys_exit(0);
}

__attribute__((naked)) void _start(void) {
    __asm__ volatile(
        "mov %esp, %eax\n"
        "push %eax\n"
        "call ls_main\n"
        "add $4, %esp\n"
        "mov $0, %ebx\n"
        "mov $2, %eax\n"
        "int $0x80\n"
        "hlt\n"
    );
}
