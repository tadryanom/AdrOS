ENTRY(start)                /* Defines the label for the kernel entry point */
SECTIONS {
    . = 0x00100000;          /* Here it says where the kernel code will be loaded into memory, in this case, over 1 MB */
    .text ALIGN (0x1000) :   /* Aligns in 4 KB */
    {
        *(.multiboot_header) /* Multiboot header on start.s file */
        *(.text)             /* All code sections of all files */
    }
    .rodata ALIGN (0x1000) : /* Aligns in 4 KB */
    {
        *(.rodata*)          /* All read-only data sections of all files */
    }
    .data ALIGN (0x1000) :   /* Aligns in 4 KB */
    {
        *(.data)             /* All data sections of all files */
    }
    .bss ALIGN (0x1000) :    /* Aligns in 4 KB */
    {
        *(COMMON)            /* All COMMON sections of all files */
        *(.bss)              /* All bss (uninitialized data) sections of all files */
        *(.kernel_stack)     /* Kernel stack on start.s file */
    }
}
