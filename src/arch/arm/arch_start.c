#include "arch/arch_start.h"
#include "kernel/boot_info.h"

#include "uart_console.h"

extern void kernel_main(const struct boot_info* bi);

void arch_start(const struct arch_boot_args* args) {
    (void)args;

    uart_init();
    uart_print("\n[AdrOS] Booting...\n");

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
