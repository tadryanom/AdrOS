/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

/*
 * AdrOS - MIPS32 Linker Script
 * Target: QEMU malta (RAM typically starts at 0x80000000, KSEG0)
 */

ENTRY(_start)

SECTIONS
{
    . = 0x80000000;

    _start = .;

    .text : {
        *(.text)
    }

    .rodata : {
        *(.rodata)
    }

    .data : {
        *(.data)
    }

    .bss : {
        *(.bss)
        *(COMMON)
    }

    _end = .;
}
