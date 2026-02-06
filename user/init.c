#include <stdint.h>

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
    return ret;
}

__attribute__((noreturn)) static void sys_exit(int code) {
    (void)code;
    __asm__ volatile(
        "int $0x80\n"
        "1: jmp 1b\n"
        :
        : "a"(SYSCALL_EXIT)
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
    sys_exit(0);
}
