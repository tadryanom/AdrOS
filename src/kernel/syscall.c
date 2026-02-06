// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#include "syscall.h"
#include "idt.h"
#include "uart_console.h"

static void syscall_handler(struct registers* regs) {
    uint32_t syscall_no = regs->eax;

    if (syscall_no == SYSCALL_WRITE) {
        uint32_t fd = regs->ebx;
        const char* buf = (const char*)regs->ecx;
        uint32_t len = regs->edx;

        if (fd != 1 && fd != 2) {
            regs->eax = (uint32_t)-1;
            return;
        }

        for (uint32_t i = 0; i < len; i++) {
            char c = buf[i];
            uart_put_char(c);
        }

        regs->eax = len;
        return;
    }

    if (syscall_no == SYSCALL_GETPID) {
        regs->eax = 0;
        return;
    }

    if (syscall_no == SYSCALL_EXIT) {
        uart_print("[USER] exit()\n");
        for(;;) {
            __asm__ volatile("cli; hlt");
        }
    }

    regs->eax = (uint32_t)-1;
}

void syscall_init(void) {
#if defined(__i386__)
    register_interrupt_handler(128, syscall_handler);
#endif
}
