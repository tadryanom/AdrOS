 #include "arch/arch_early_setup.h"
#include "kernel/boot_info.h"

#include "uart_console.h"
#include "console.h"

extern void kernel_main(const struct boot_info* bi);

 void arch_early_setup(const struct arch_boot_args* args) {
    (void)args;

    uart_init();
    kprintf("\n[AdrOS/riscv64] Booting on QEMU virt...\n");

    struct boot_info bi;
    bi.arch_magic = 0;
    bi.arch_boot_info = 0;
    bi.initrd_start = 0;
    bi.initrd_end = 0;
    bi.cmdline = NULL;

    kernel_main(&bi);

    for(;;) {
        __asm__ volatile("wfi");
    }
}
