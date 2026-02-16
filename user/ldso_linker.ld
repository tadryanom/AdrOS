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
