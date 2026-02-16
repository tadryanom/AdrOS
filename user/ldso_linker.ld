/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

ENTRY(_start)

SECTIONS
{
    . = 0x12000000;

    .text : {
        *(.text.start)
        *(.text .text.*)
    }

    .rodata : { *(.rodata*) }

    . = ALIGN(0x1000);

    .data : { *(.data*) }

    .bss : {
        *(.bss*)
        *(COMMON)
    }
}
