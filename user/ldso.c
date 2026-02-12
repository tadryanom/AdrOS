/* Minimal userspace dynamic linker stub (ld.so).
 * Currently a no-op: receives control from the kernel ELF loader
 * and immediately jumps to the real program entry point.
 *
 * The kernel places the program entry address at the top of the
 * user stack (below argc) as an auxiliary value.
 *
 * Future work: parse PT_DYNAMIC, relocate GOT/PLT, resolve symbols. */

void _start(void) __attribute__((noreturn, section(".text.start")));

void _start(void) {
    /* The kernel's ELF loader doesn't yet pass AT_ENTRY via auxv,
     * so for now this stub simply halts. When the kernel is extended
     * to pass the real entry point (e.g. via auxv or a register),
     * this stub will jump there.
     *
     * Placeholder: infinite NOP loop (cannot hlt in ring 3). */
    for (;;) {
        __asm__ volatile("nop");
    }
}
