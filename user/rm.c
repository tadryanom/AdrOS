/* AdrOS rm utility */
#include <stdint.h>
#include "user_errno.h"

enum {
    SYSCALL_WRITE  = 1,
    SYSCALL_EXIT   = 2,
    SYSCALL_UNLINK = 29,
    SYSCALL_RMDIR  = 40,
};

static int sys_write(int fd, const void* buf, uint32_t len) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret)
        : "a"(SYSCALL_WRITE), "b"(fd), "c"(buf), "d"(len) : "memory");
    return __syscall_fix(ret);
}

static int sys_unlink(const char* path) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret)
        : "a"(SYSCALL_UNLINK), "b"(path) : "memory");
    return __syscall_fix(ret);
}

static int sys_rmdir(const char* path) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret)
        : "a"(SYSCALL_RMDIR), "b"(path) : "memory");
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

static int scmp(const char* a, const char* b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

static void rm_main(uint32_t* sp0) {
    uint32_t argc = sp0 ? sp0[0] : 0;
    const char* const* argv = (const char* const*)(sp0 + 1);

    if (argc <= 1) {
        wr(2, "rm: missing operand\n");
        sys_exit(1);
    }

    int rflag = 0;
    int rc = 0;
    uint32_t start = 1;

    if (argv[1] && (scmp(argv[1], "-r") == 0 || scmp(argv[1], "-rf") == 0 ||
                    scmp(argv[1], "-d") == 0)) {
        rflag = 1;
        start = 2;
    }

    for (uint32_t i = start; i < argc; i++) {
        int r = sys_unlink(argv[i]);
        if (r < 0 && rflag) {
            r = sys_rmdir(argv[i]);
        }
        if (r < 0) {
            wr(2, "rm: cannot remove '");
            wr(2, argv[i]);
            wr(2, "'\n");
            rc = 1;
        }
    }
    sys_exit(rc);
}

__attribute__((naked)) void _start(void) {
    __asm__ volatile(
        "mov %esp, %eax\n"
        "push %eax\n"
        "call rm_main\n"
        "add $4, %esp\n"
        "mov $0, %ebx\n"
        "mov $2, %eax\n"
        "int $0x80\n"
        "hlt\n"
    );
}
