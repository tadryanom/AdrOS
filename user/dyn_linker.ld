/* Linker script for dynamically-linked AdrOS userspace commands.
 * These executables use /lib/ld.so as interpreter and link against libc.so. */
ENTRY(_start)

SECTIONS
{
    . = 0x00400000;

    .interp : { *(.interp) }
    .hash   : { *(.hash) }
    .dynsym : { *(.dynsym) }
    .dynstr : { *(.dynstr) }
    .rel.dyn : { *(.rel.dyn) }
    .rel.plt : { *(.rel.plt) }
    .plt    : { *(.plt) *(.plt.*) }

    .text : {
        *(.text .text.*)
    }

    .rodata : { *(.rodata*) }

    . = ALIGN(0x1000);

    .dynamic : { *(.dynamic) }
    .got.plt : { *(.got.plt) }
    .got     : { *(.got) }

    .data : { *(.data*) }

    .bss : {
        *(.bss*)
        *(COMMON)
    }
}
