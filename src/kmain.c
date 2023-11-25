#include <kmain.h>

//#define RED   0xFF0000
//#define GREEN 0xFF00
//#define BLUE  0xFF

/*
 * Kernel main function
 * Here w'll starting all kernel services
 */
void kmain (u64int magic, u64int addr)
{
    init_video();
    /* Am I booted by a Multiboot-compliant boot loader? */
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        printf ("Invalid magic number: 0x%x\n", (unsigned) magic);
        return;
    }
    mbi = (multiboot_info_t *) addr;

    printmbi();

    s32int cr0_value = 0;
    asm volatile ("movl %%cr0, %0" : "=r" (cr0_value));
    printf("cr0_value = 0x%x\n", cr0_value);

    for (s32int i = 0; i < 1500; i++){
        puts(".");
        for (u64int j = 0; j < 10000000; j++){}
    }
    puts("OK\n");
    //putpixel(511, 383, RED);
    //putpixel(515, 383, GREEN);
}

/*
 * Print Multiboot Info
 */
void printmbi(void)
{
    /* Print out the flags. */
    printf ("flags = 0x%x\n", (unsigned) mbi->flags);

    /* Are mem_* valid? */
    if (CHECK_FLAG (mbi->flags, 0))
        printf ("mem_lower = %uKB, mem_upper = %uKB\n",
            (unsigned) mbi->mem_lower, (unsigned) mbi->mem_upper);

    /* Is boot_device valid? */
    if (CHECK_FLAG (mbi->flags, 1))
        printf ("boot_device = 0x%x\n", (unsigned) mbi->boot_device);

    /* Is the command line passed? */
    if (CHECK_FLAG (mbi->flags, 2))
        printf ("cmdline = %s\n", (s8int *) mbi->cmdline);

    /* Are mods_* valid? */
    if (CHECK_FLAG (mbi->flags, 3)) {
        multiboot_module_t *mod;
        s32int i;

        printf ("mods_count = %d, mods_addr = 0x%x\n",
            (s32int) mbi->mods_count, (s32int) mbi->mods_addr);
        for (i = 0, mod = (multiboot_module_t *) mbi->mods_addr;
            i < (s32int)mbi->mods_count;
            i++, mod++)
            printf (" mod_start = 0x%x, mod_end = 0x%x, cmdline = %s\n",
                (unsigned) mod->mod_start,
                (unsigned) mod->mod_end,
                (s8int *) mod->cmdline);
    }

    /* Is the section header table of ELF valid? */
    if (CHECK_FLAG (mbi->flags, 5)) {
        multiboot_elf_section_header_table_t *multiboot_elf_sec = &(mbi->u.elf_sec);

        printf ("multiboot_elf_sec: num = %u, size = 0x%x,"
            " addr = 0x%x, shndx = 0x%x\n",
            (unsigned) multiboot_elf_sec->num, (unsigned) multiboot_elf_sec->size,
            (unsigned) multiboot_elf_sec->addr, (unsigned) multiboot_elf_sec->shndx);
    }

    /* Are mmap_* valid? */
    if (CHECK_FLAG (mbi->flags, 6)) {
        multiboot_memory_map_t *mmap;

    printf ("mmap_addr = 0x%x, mmap_length = 0x%x\n",
        (unsigned) mbi->mmap_addr, (unsigned) mbi->mmap_length);
    for (mmap = (multiboot_memory_map_t *) mbi->mmap_addr;
        (u64int) mmap < mbi->mmap_addr + mbi->mmap_length;
        mmap = (multiboot_memory_map_t *) ((u64int) mmap
            + mmap->size + sizeof (mmap->size)))
        printf (" size = 0x%x, base_addr = 0x%x%08x,"
            " length = 0x%x%08x, type = 0x%x\n",
            (unsigned) mmap->size,
            (unsigned) (mmap->addr >> 32),
            (unsigned) (mmap->addr & 0xffffffff),
            (unsigned) (mmap->len >> 32),
            (unsigned) (mmap->len & 0xffffffff),
            (unsigned) mmap->type);
    }
}

/* Print a pixel on screen */
void putpixel(s32int pos_x, s32int pos_y, s32int color)
{
    if (CHECK_FLAG (mbi->flags, 12)) {
        void *fb = (void *) (u64int) mbi->framebuffer_addr;
        multiboot_uint32_t *pixel
            = fb + mbi->framebuffer_pitch * pos_y + 4 * pos_x;
        *pixel = color;
    }
}
