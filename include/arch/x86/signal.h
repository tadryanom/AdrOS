// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#ifndef ARCH_X86_SIGNAL_H
#define ARCH_X86_SIGNAL_H

#include <stdint.h>
#include "arch/x86/idt.h"

#define SIGFRAME_MAGIC 0x53494746U /* 'SIGF' */

struct sigframe {
    uint32_t magic;
    struct registers saved;
};

#endif /* ARCH_X86_SIGNAL_H */
