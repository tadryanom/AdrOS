/* Minimal userspace dynamic linker (ld.so).
 *
 * The kernel ELF loader pushes an auxiliary vector (auxv) onto the user
 * stack when PT_INTERP is present.  This linker parses auxv to find
 * AT_ENTRY (the real program entry point), then jumps there.
 *
 * The kernel already performs eager relocation of R_386_RELATIVE,
 * R_386_GLOB_DAT, R_386_JMP_SLOT, and R_386_32 before transferring
 * control, so no additional relocation processing is needed here.
 *
 * Future work: lazy PLT binding via GOT[2] resolver trampoline. */

#define AT_NULL   0
#define AT_ENTRY  9

typedef unsigned int uint32_t;

struct auxv_entry {
    uint32_t a_type;
    uint32_t a_val;
};

void _start(void) __attribute__((noreturn, naked, section(".text.start")));

void _start(void) {
    __asm__ volatile(
        /* ESP points to the auxv array pushed by the kernel.
         * Scan for AT_ENTRY (type 9) to find the real program entry. */
        "mov %%esp, %%esi\n"       /* esi = auxv pointer */
    "1:\n"
        "mov 0(%%esi), %%eax\n"    /* eax = a_type */
        "test %%eax, %%eax\n"      /* AT_NULL? */
        "jz 2f\n"
        "cmp $9, %%eax\n"          /* AT_ENTRY? */
        "je 3f\n"
        "add $8, %%esi\n"          /* next entry */
        "jmp 1b\n"
    "3:\n"
        "mov 4(%%esi), %%eax\n"    /* eax = AT_ENTRY value */
        "jmp *%%eax\n"             /* jump to real program entry */
    "2:\n"
        /* AT_ENTRY not found — exit(127) */
        "mov $2, %%eax\n"          /* SYSCALL_EXIT */
        "mov $127, %%ebx\n"
        "int $0x80\n"
    "3:\n"
        "jmp 3b\n"
        ::: "eax", "esi", "memory"
    );
}
