/* AdrOS cat utility */
#include <stdint.h>
#include "user_errno.h"

enum {
    SYSCALL_WRITE = 1,
    SYSCALL_EXIT  = 2,
    SYSCALL_OPEN  = 4,
    SYSCALL_READ  = 5,
    SYSCALL_CLOSE = 6,
};

static int sys_write(int fd, const void* buf, uint32_t len) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret)
        : "a"(SYSCALL_WRITE), "b"(fd), "c"(buf), "d"(len) : "memory");
    return __syscall_fix(ret);
}

static int sys_read(int fd, void* buf, uint32_t len) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret)
        : "a"(SYSCALL_READ), "b"(fd), "c"(buf), "d"(len) : "memory");
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

static void cat_fd(int fd) {
    char buf[256];
    int r;
    while ((r = sys_read(fd, buf, sizeof(buf))) > 0) {
        sys_write(1, buf, (uint32_t)r);
    }
}

static void cat_main(uint32_t* sp0) {
    uint32_t argc = sp0 ? sp0[0] : 0;
    const char* const* argv = (const char* const*)(sp0 + 1);

    if (argc <= 1) {
        cat_fd(0);
        sys_exit(0);
    }

    int rc = 0;
    for (uint32_t i = 1; i < argc; i++) {
        int fd = sys_open(argv[i], 0);
        if (fd < 0) {
            wr(2, "cat: ");
            wr(2, argv[i]);
            wr(2, ": No such file\n");
            rc = 1;
            continue;
        }
        cat_fd(fd);
        sys_close(fd);
    }
    sys_exit(rc);
}

__attribute__((naked)) void _start(void) {
    __asm__ volatile(
        "mov %esp, %eax\n"
        "push %eax\n"
        "call cat_main\n"
        "add $4, %esp\n"
        "mov $0, %ebx\n"
        "mov $2, %eax\n"
        "int $0x80\n"
        "hlt\n"
    );
}
