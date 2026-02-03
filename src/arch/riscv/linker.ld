/*
 * AdrOS - RISC-V Linker Script
 * Target: QEMU virt (RAM starts at 0x80000000)
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
