// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

#include <stdint.h>
#include <stddef.h>

#include "console.h"
#include "arch/x86/usermode.h"
#include "hal/usermode.h"
#include "arch/x86/idt.h"

#if defined(__i386__)

/* User pages can be anywhere in physical memory on 32-bit PAE. */

__attribute__((noreturn)) void x86_enter_usermode(uintptr_t user_eip, uintptr_t user_esp) {
    kprintf("[USER] enter ring3 eip=0x%x esp=0x%x\n",
            (unsigned)user_eip, (unsigned)user_esp);

    __asm__ volatile(
        "cli\n"
        "mov $0x23, %%ax\n"     /* user data segment (GDT entry 4, RPL=3) */
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "pushl $0x23\n"         /* ss */
        "pushl %[esp]\n"        /* esp */
        "pushl $0x202\n"        /* eflags: IF=1 */
        "pushl $0x1B\n"         /* cs */
        "pushl %[eip]\n"        /* eip */
        "iret\n"
        :
        : [eip] "r"(user_eip), [esp] "r"(user_esp)
        : "memory", "eax"
    );

    __builtin_unreachable();
}

__attribute__((noreturn)) void x86_enter_usermode_regs(const struct registers* regs) {
    if (!regs) {
        for (;;) {
            __asm__ volatile("cli; hlt");
        }
    }

    // Layout follows include/arch/x86/idt.h struct registers.
    // struct registers { gs(0), ds(4), edi(8), esi(12), ebp(16),
    //   esp(20), ebx(24), edx(28), ecx(32), eax(36),
    //   int_no(40), err_code(44), eip(48), cs(52), eflags(56),
    //   useresp(60), ss(64) };
    const uint32_t eflags = (regs->eflags | 0x200U);

    /* Use ESI as scratch to hold regs pointer, since we'll overwrite
     * EBP manually inside the asm block. ESI is restored from the
     * struct before iret. */
    __asm__ volatile(
        "cli\n"
        "mov %[r], %%esi\n"

        "mov $0x23, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"

        "pushl $0x23\n"           /* ss */
        "pushl 60(%%esi)\n"       /* useresp */
        "pushl %[efl]\n"          /* eflags */
        "pushl $0x1B\n"           /* cs */
        "pushl 48(%%esi)\n"       /* eip */

        "mov  8(%%esi), %%edi\n"  /* edi */
        "mov 16(%%esi), %%ebp\n"  /* ebp */
        "mov 24(%%esi), %%ebx\n"  /* ebx */
        "mov 28(%%esi), %%edx\n"  /* edx */
        "mov 32(%%esi), %%ecx\n"  /* ecx */
        "mov 36(%%esi), %%eax\n"  /* eax */
        "mov 12(%%esi), %%esi\n"  /* esi (last — self-overwrite) */
        "iret\n"
        :
        : [r] "r"(regs),
          [efl] "r"(eflags)
        : "memory", "cc"
    );

    __builtin_unreachable();
}

#endif
