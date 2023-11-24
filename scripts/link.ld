ENTRY(kernel_entry)                        /* Defines the label for the kernel entry point */
SECTIONS {
    . = 0x00200000;                        /* Here it says where the kernel code will be loaded into memory, in this case, over 2 MB */
    .text BLOCK(0x1000) : ALIGN (0x1000)   /* Aligns in 4 KB */
    {
        *(.multiboot)                      /* Multiboot header on start.s file */
        *(.text)                           /* All code sections of all files */
    }
    .rodata BLOCK(0x1000) : ALIGN (0x1000) /* Aligns in 4 KB */
    {
        *(.rodata*)                        /* All read-only data sections of all files */
    }
    .data BLOCK(0x1000) : ALIGN (0x1000)   /* Aligns in 4 KB */
    {
        *(.data)                           /* All data sections of all files */
    }
    .bss BLOCK(0x1000) : ALIGN (0x1000)    /* Aligns in 4 KB */
    {
        *(COMMON)                          /* All COMMON sections of all files */
        *(.bss)                            /* All bss (uninitialized data) sections of all files */
    }
}
