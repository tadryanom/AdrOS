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

void _start(void) {
    static const char m[] = "[echo] hello from echo.elf\n";
    (void)sys_write(1, m, (uint32_t)(sizeof(m) - 1));
    sys_exit(0);
}
