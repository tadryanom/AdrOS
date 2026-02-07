// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#include "hal/usermode.h"

#if defined(__i386__)
#include "arch/x86/usermode.h"
#endif

int hal_usermode_enter(uintptr_t user_eip, uintptr_t user_esp) {
#if defined(__i386__)
    x86_enter_usermode(user_eip, user_esp);
#else
    (void)user_eip;
    (void)user_esp;
    return -1;
#endif
}
