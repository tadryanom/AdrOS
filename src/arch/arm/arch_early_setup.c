// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

 #include "arch/arch_early_setup.h"
#include "kernel/boot_info.h"

#include "uart_console.h"

extern void kernel_main(const struct boot_info* bi);

 void arch_early_setup(const struct arch_boot_args* args) {
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
