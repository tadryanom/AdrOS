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
