/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

/*
 * AdrOS - ARM64 Linker Script
 * Target: QEMU virt (RAM starts at 0x40000000 usually)
 */

ENTRY(_start)

SECTIONS
{
    . = 0x40000000;
    
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
