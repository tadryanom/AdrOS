/* PIE test binary for PLT/GOT lazy binding verification.
 * Calls test_add() from libpietest.so through PLT — resolved lazily by ld.so.
 * Built as: i686-elf-ld -pie --dynamic-linker=/lib/ld.so */

static inline void sys_exit(int code) {
    __asm__ volatile("int $0x80" :: "a"(2), "b"(code) : "memory");
}

static inline int sys_write(int fd, const void* buf, unsigned len) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(1), "b"(fd), "c"(buf), "d"(len) : "memory");
    return ret;
}

extern int test_add(int a, int b);

void _start(void) {
    int r = test_add(38, 4);
    if (r == 42) {
        sys_write(1, "[init] lazy PLT OK\n", 19);
    } else {
        sys_write(1, "[init] lazy PLT FAIL\n", 21);
    }

    /* Call again — this time GOT is already patched, tests direct path */
    r = test_add(100, 23);
    if (r == 123) {
        sys_write(1, "[init] PLT cached OK\n", 21);
    } else {
        sys_write(1, "[init] PLT cached FAIL\n", 23);
    }

    sys_exit(0);
}
