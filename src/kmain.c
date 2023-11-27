#include <kmain.h>

//#define RED   0xFF0000
//#define GREEN 0xFF00
//#define BLUE  0xFF

// Check if the bit BIT in FLAGS is set.
#define CHECK_FLAG(flags,bit) ((flags) & (1 << (bit)))

// Forward declarations.
static void printmbi (multiboot_info_t *);
//static void putpixel (s32int, s32int, s32int, multiboot_info_t *);

extern u32int placement_address;
u32int initial_esp;

/*
 * Kernel main function 
 * Here w'll starting all kernel services
 */
void kmain (u64int magic, u64int addr, u32int initial_stack)
{
    // Initialise the screen (by clearing it)
    init_video();
    // Am I booted by a Multiboot-compliant boot loader?
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        printf ("Invalid magic number: 0x%x\n", (unsigned) magic);
        return;
    }
    multiboot_info_t *mbi = (multiboot_info_t *) addr;

    printmbi(mbi);

    s32int cr0_value = 0;
    asm volatile ("movl %%cr0, %0" : "=r" (cr0_value));
    printf("cr0_value = 0x%08x\n", cr0_value);

    initial_esp = initial_stack;
    printf("initial_esp = 0x%08x\n", initial_esp);

    // Initialise all the ISRs and segmentation
    init_descriptors();
    // Initialise the PIT to 1000Hz
    init_timer(1000);
	init_rtc();
    asm volatile("sti");

    // Find the location of our initial ramdisk.
    ASSERT(mbi->mods_count > 0);
    u32int initrd_location = *((u32int*)mbi->mods_addr);
    u32int initrd_end = *(u32int*)(mbi->mods_addr + 4);
    // Don't trample our module with placement accesses, please!
    placement_address = initrd_end;
    printf("initrd_location = 0x%04x - initrd_end = 0x%04x\n", initrd_location, placement_address);

    // Start paging.
    initialise_paging();

    // Start multitasking.
    //initialise_tasking();

    // Initialise the initial ramdisk, and set it as the filesystem root.
    fs_root = initialise_initrd(initrd_location);

    // Create a new process in a new address space which is a clone of this.
    /*s32int ret = fork();
    printf("fork() returned %d, and getpid() returned %d\
        \n============================================================================\n \
    ", ret, getpid());*/

    // The next section of code is not reentrant so make sure we aren't interrupted during.
    //asm volatile("cli");
    // list the contents of /
	s32int i = 0;
	struct dirent *node = 0;
	//u8int * buf = (u8int*)kmalloc(sizeof(u8int) * 1024);
	while ((node = readdir_fs(fs_root, i)) != 0) {
		printf("Found file %s", node->name);
		fs_node_t *fsnode = finddir_fs(fs_root, node->name);
		if ((fsnode->flags&0x7) == FS_DIRECTORY)
			puts("\n\t(directory)\n");
		else {
			puts("\n\t contents: \"");
			u8int buf[1024];
			//memset(buf, 0, 1024);
			u32int sz = read_fs(fsnode, 0, 1024, buf);
			s32int j;
			for (j = 0; (u32int)j < sz; j++)
				printf("%c", buf[j]);
			puts("\"\n");
		}
		i++;
	}
    //puts("/n");
	//kfree(buf);
    //asm volatile("sti");

    //for (s32int i = 0; i < 1500; i++) {
	for (s32int i = 0; i < 15; i++){
        puts(".");
        //for (u64int j = 0; j < 10000000; j++){}
        sleep_ms(1000);
    }
    puts("OK\n");

    //asm volatile ("int $0x3");
    //asm volatile ("int $0x4");

    //putpixel(511, 383, RED);
    //putpixel(515, 383, GREEN);
}

// Print Multiboot Info
static void printmbi (multiboot_info_t *multiboot_info)
{
    // Print out the flags.
    printf ("flags = 0x%x\n", (unsigned) multiboot_info->flags);

    // Are mem_* valid?
    if (CHECK_FLAG (multiboot_info->flags, 0))
        printf ("mem_lower = %uKB, mem_upper = %uKB\n",
            (unsigned) multiboot_info->mem_lower, (unsigned) multiboot_info->mem_upper);

    // Is boot_device valid?
    if (CHECK_FLAG (multiboot_info->flags, 1))
        printf ("boot_device = 0x%x\n", (unsigned) multiboot_info->boot_device);

    // Is the command line passed?
    if (CHECK_FLAG (multiboot_info->flags, 2))
        printf ("cmdline = %s\n", (s8int *) multiboot_info->cmdline);

    // Are mods_* valid?
    if (CHECK_FLAG (multiboot_info->flags, 3)) {
        multiboot_module_t *mod;
        s32int i;

        printf ("mods_count = %d, mods_addr = 0x%x\n",
            (s32int) multiboot_info->mods_count, (s32int) multiboot_info->mods_addr);
        for (i = 0, mod = (multiboot_module_t *) multiboot_info->mods_addr;
            i < (s32int)multiboot_info->mods_count;
            i++, mod++)
            printf (" mod_start = 0x%x, mod_end = 0x%x, cmdline = %s\n",
                (unsigned) mod->mod_start,
                (unsigned) mod->mod_end,
                (s8int *) mod->cmdline);
    }

    // Is the section header table of ELF valid?
    if (CHECK_FLAG (multiboot_info->flags, 5)) {
        multiboot_elf_section_header_table_t *multiboot_elf_sec = &(multiboot_info->u.elf_sec);

        printf ("multiboot_elf_sec: num = %u, size = 0x%x,"
            " addr = 0x%x, shndx = 0x%x\n",
            (unsigned) multiboot_elf_sec->num, (unsigned) multiboot_elf_sec->size,
            (unsigned) multiboot_elf_sec->addr, (unsigned) multiboot_elf_sec->shndx);
    }

    // Are mmap_* valid?
    if (CHECK_FLAG (multiboot_info->flags, 6)) {
        multiboot_memory_map_t *mmap;

    printf ("mmap_addr = 0x%x, mmap_length = 0x%x\n",
        (unsigned) multiboot_info->mmap_addr, (unsigned) multiboot_info->mmap_length);
    for (mmap = (multiboot_memory_map_t *) multiboot_info->mmap_addr;
        (u64int) mmap < multiboot_info->mmap_addr + multiboot_info->mmap_length;
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

// Print a pixel on screen
/*static void putpixel (s32int pos_x, s32int pos_y, s32int color, multiboot_info_t *multiboot_info)
{
    if (CHECK_FLAG (multiboot_info->flags, 12)) {
        void *fb = (void *) (u64int) multiboot_info->framebuffer_addr;
        multiboot_uint32_t *pixel
            = fb + multiboot_info->framebuffer_pitch * pos_y + 4 * pos_x;
        *pixel = color;
    }
}*/
