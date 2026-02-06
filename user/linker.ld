ENTRY(_start)

PHDRS
{
    text PT_LOAD FLAGS(7);
}

SECTIONS
{
    . = 0x00400000;

    .text : {
        *(.text*)
    } :text

    .rodata : {
        *(.rodata*)
    } :text

    .data : {
        *(.data*)
    } :text

    .bss : {
        *(.bss*)
        *(COMMON)
    } :text
}
