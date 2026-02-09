#include <stdint.h>

#include "user_errno.h"

enum {
    SYSCALL_WRITE = 1,
    SYSCALL_EXIT  = 2,
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

static uint32_t ustrlen(const char* s) {
    uint32_t n = 0;
    while (s && s[n]) n++;
    return n;
}

static void write_str(const char* s) {
    (void)sys_write(1, s, ustrlen(s));
}

static void u32_to_dec(uint32_t v, char out[16]) {
    char tmp[16];
    uint32_t n = 0;
    if (v == 0) {
        out[0] = '0';
        out[1] = 0;
        return;
    }
    while (v && n < sizeof(tmp) - 1) {
        tmp[n++] = (char)('0' + (v % 10));
        v /= 10;
    }
    uint32_t i = 0;
    while (n) out[i++] = tmp[--n];
    out[i] = 0;
}

static __attribute__((noreturn)) void sys_exit(int status) {
    __asm__ volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_EXIT), "b"(status)
        : "memory"
    );
    for (;;) {
        __asm__ volatile("hlt");
    }
}

static void echo_main(uint32_t* sp0) {
    write_str("[echo] hello from echo.elf\n");

    uint32_t argc = sp0 ? sp0[0] : 0;
    const char* const* argv = (const char* const*)(sp0 + 1);

    const char* const* envp = argv;
    for (uint32_t i = 0; argv && argv[i]; i++) {
        envp = &argv[i + 1];
    }
    if (envp) envp++;

    char num[16];
    u32_to_dec(argc, num);
    write_str("[echo] argc=");
    write_str(num);
    write_str("\n");

    if (argv && argv[0]) {
        write_str("[echo] argv0=");
        write_str(argv[0]);
        write_str("\n");
    }
    if (argv && argv[1]) {
        write_str("[echo] argv1=");
        write_str(argv[1]);
        write_str("\n");
    }
    if (argv && argv[2]) {
        write_str("[echo] argv2=");
        write_str(argv[2]);
        write_str("\n");
    }
    if (envp && envp[0]) {
        write_str("[echo] env0=");
        write_str(envp[0]);
        write_str("\n");
    }
    sys_exit(0);
}

__attribute__((naked)) void _start(void) {
    __asm__ volatile(
        "mov %esp, %eax\n"
        "push %eax\n"
        "call echo_main\n"
        "add $4, %esp\n"
        "mov $0, %ebx\n"
        "mov $2, %eax\n"
        "int $0x80\n"
        "hlt\n"
    );
}
