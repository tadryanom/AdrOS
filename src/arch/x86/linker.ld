/*
 * AdrOS - x86 Linker Script (Higher Half)
 */

/* The entry point reverts to the default _start */
ENTRY(_start)

/* The bootloader loads us at 1MB physical */
PHYSICAL_BASE = 0x00100000;

/* But we want to run at 3GB + 1MB virtual */
KERNEL_VIRT_BASE = 0xC0000000;

SECTIONS
{
    /* 
     * We start at 1MB physical. 
     */
    . = PHYSICAL_BASE;

    /** TEXT SECTION:
     * VMA = 0xC0100000 (3GB + 1MB)
     * LMA = 0x00100000 (1MB) - Defined by AT()
     */
    .text KERNEL_VIRT_BASE + PHYSICAL_BASE : AT(PHYSICAL_BASE)
    {
        /* REQUIRED: The Header MUST be the first thing */
        KEEP(*(.multiboot_header))
        /* Boot code (boot.o) */
        *(.boot_text)
        /* Rest of the kernel code */
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
