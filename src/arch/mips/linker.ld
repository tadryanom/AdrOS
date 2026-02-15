/*
 * AdrOS - MIPS32r2 Linker Script
 * Target: QEMU Malta board (KSEG0, RAM at 0x80000000)
 */

ENTRY(_start)

SECTIONS
{
    . = 0x80100000;

    .text : {
        *(.text .text.*)
    }

    . = ALIGN(4096);
    .rodata : {
        *(.rodata .rodata.*)
    }

    . = ALIGN(4096);
    .data : {
        *(.data .data.*)
    }

    . = ALIGN(4096);
    __bss_start = .;
    .bss : {
        *(.bss .bss.*)
        *(COMMON)
    }
    __bss_end = .;

    . = ALIGN(4096);
    _end = .;

    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
        *(.gnu.hash)
        *(.dynsym)
        *(.dynstr)
        *(.MIPS.abiflags)
        *(.reginfo)
    }
}
