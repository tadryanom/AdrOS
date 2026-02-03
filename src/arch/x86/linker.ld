/*
 * AdrOS - x86 Linker Script (Higher Half)
 */

ENTRY(_start)

/* The bootloader loads us at 1MB physical */
PHYSICAL_BASE = 0x00100000;

/* But we want to run at 3GB + 1MB virtual */
KERNEL_VIRT_BASE = 0xC0000000;

SECTIONS
{
    /* 
     * We start at 1MB physical. 
     * The symbol '.' tracks the VIRTUAL address.
     * We offset it to 3GB + 1MB.
     */
    . = KERNEL_VIRT_BASE + PHYSICAL_BASE;

    _start = .;

    /* 
     * The AT(...) keyword tells the linker:
     * "Put this section at PHYSICAL address X in the file, 
     *  but calculate symbols as if it were at VIRTUAL address Y"
     */

    .text : AT(ADDR(.text) - KERNEL_VIRT_BASE)
    {
        /* Multiboot header must be very early physically */
        KEEP(*(.multiboot_header))
        *(.text)
    }

    .rodata : AT(ADDR(.rodata) - KERNEL_VIRT_BASE)
    {
        *(.rodata)
    }

    .data : AT(ADDR(.data) - KERNEL_VIRT_BASE)
    {
        *(.data)
    }

    .bss : AT(ADDR(.bss) - KERNEL_VIRT_BASE)
    {
        *(.bss)
        *(COMMON)
    }

    _end = .;
}
