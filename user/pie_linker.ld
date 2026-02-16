ENTRY(_start)

SECTIONS
{
    . = 0x10000000;

    .interp : { *(.interp) }
    .hash   : { *(.hash) }
    .dynsym : { *(.dynsym) }
    .dynstr : { *(.dynstr) }
    .rel.plt : { *(.rel.plt) }
    .plt    : { *(.plt) *(.plt.*) }

    .text : {
        *(.text.start)
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
