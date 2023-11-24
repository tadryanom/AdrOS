#include <multiboot.h>

/* Check if the bit BIT in FLAGS is set. */
#define CHECK_FLAG(flags,bit) ((flags) & (1 << (bit)))

/* Some screen stuff. */
#define COLUMNS   80
#define LINES     24
#define ATTRIBUTE 7
#define VIDEO     0xB8000 /* The video memory address. */

#define RED   0xFF0000
#define GREEN 0xFF00
#define BLUE  0xFF

static int xpos;
static int ypos;
static volatile unsigned char *video;

static multiboot_info_t *mbi;

/* Forward declarations. */
void kmain (unsigned long magic, unsigned long addr);
void putpixel(int pos_x, int pos_y, int color); 
void sgrubi (void);
void drawline (void);
//static void cls (void);
void cls (void);
static void itoa (char *buf, int base, int d);
static void putchar (int c);
void printf (const char *format, ...);

void kmain (unsigned long magic, unsigned long addr)
{
    /* Am I booted by a Multiboot-compliant boot loader? */
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        printf ("Invalid magic number: 0x%x\n", (unsigned) magic);
        return;
    } else {
        /* Set MBI to the address of the Multiboot information structure. */
        mbi = (multiboot_info_t *) addr;
    }

    putpixel(511, 383, RED);
    putpixel(515, 383, GREEN);
}

void putpixel(int pos_x, int pos_y, int color)
{
    if (CHECK_FLAG (mbi->flags, 12)) {
        void *fb = (void *) (unsigned long) mbi->framebuffer_addr;
        multiboot_uint32_t *pixel
            = fb + mbi->framebuffer_pitch * pos_y + 4 * pos_x;
        *pixel = color;
    }
}

/* Show grub info. */
void sgrubi (void)
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
        printf ("cmdline = %s\n", (char *) mbi->cmdline);

    /* Are mods_* valid? */
    if (CHECK_FLAG (mbi->flags, 3)) {
        multiboot_module_t *mod;
        int i;

        printf ("mods_count = %d, mods_addr = 0x%x\n",
            (int) mbi->mods_count, (int) mbi->mods_addr);
        for (i = 0, mod = (multiboot_module_t *) mbi->mods_addr;
            i < (int)mbi->mods_count;
            i++, mod++)
            printf (" mod_start = 0x%x, mod_end = 0x%x, cmdline = %s\n",
                (unsigned) mod->mod_start,
                (unsigned) mod->mod_end,
                (char *) mod->cmdline);
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
        (unsigned long) mmap < mbi->mmap_addr + mbi->mmap_length;
        mmap = (multiboot_memory_map_t *) ((unsigned long) mmap
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

/* Draw pixel on screen. */
void drawline (void)
{
    if (CHECK_FLAG (mbi->flags, 12)) {
        multiboot_uint32_t color;
        unsigned i;
        void *fb = (void *) (unsigned long) mbi->framebuffer_addr;

        switch (mbi->framebuffer_type) {
            case MULTIBOOT_FRAMEBUFFER_TYPE_INDEXED: {
                unsigned best_distance, distance;
                struct multiboot_color *palette;
            
                palette = (struct multiboot_color *) mbi->framebuffer_palette_addr;
                color = 0;
                best_distance = 4*256*256;
                for (i = 0; i < mbi->framebuffer_palette_num_colors; i++) {
                    distance = (0xff - palette[i].blue) * (0xff - palette[i].blue)
                        + palette[i].red * palette[i].red
                        + palette[i].green * palette[i].green;
                    if (distance < best_distance) {
                        color = i;
                        best_distance = distance;
                    }
                }
            }
                break;
            case MULTIBOOT_FRAMEBUFFER_TYPE_RGB:
                color = ((1 << mbi->framebuffer_blue_mask_size) - 1) 
                    << mbi->framebuffer_blue_field_position;
                break;
            case MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT:
                color = '\\' | 0x0100;
                break;
            default:
                color = 0xffffffff;
                break;
        }
        for (i = 0; i < mbi->framebuffer_width
            && i < mbi->framebuffer_height; i++) {
            switch (mbi->framebuffer_bpp) {
                case 8: {
                    multiboot_uint8_t *pixel = fb + mbi->framebuffer_pitch * i + i;
                    *pixel = color;
                }
                    break;
                case 15:
                case 16: {
                    multiboot_uint16_t *pixel
                        = fb + mbi->framebuffer_pitch * i + 2 * i;
                    *pixel = color;
                }
                    break;
                case 24: {
                    multiboot_uint32_t *pixel
                        = fb + mbi->framebuffer_pitch * i + 3 * i;
                    *pixel = (color & 0xffffff) | (*pixel & 0xff000000);
                }
                    break;
                case 32: {
                    multiboot_uint32_t *pixel
                        = fb + mbi->framebuffer_pitch * i + 4 * i;
                    *pixel = color;
                }
                    break;
            }
        }
    }
}

/* Clear the screen and initialize VIDEO, XPOS and YPOS. */
//static void cls (void)
void cls (void)
{
    int i;
    video = (unsigned char *) VIDEO;
  
    for (i = 0; i < COLUMNS * LINES * 2; i++)
        *(video + i) = 0;

    xpos = 0;
    ypos = 0;
}

/* Convert the integer D to a string and save the string in BUF. If
 * BASE is equal to ’d’, interpret that D is decimal, and if BASE is
 * equal to ’x’, interpret that D is hexadecimal.
 */
static void itoa (char *buf, int base, int d)
{
    char *p = buf;
    char *p1, *p2;
    unsigned long ud = d;
    int divisor = 10;
  
    /* If %d is specified and D is minus, put ‘-’ in the head. */
    if (base == 'd' && d < 0) {
        *p++ = '-';
        buf++;
        ud = -d;
    } else if (base == 'x')
       divisor = 16;

    /* Divide UD by DIVISOR until UD == 0. */
    do {
        int remainder = ud % divisor;
        *p++ = (remainder < 10) ? remainder + '0' : remainder + 'a' - 10;
    } while (ud /= divisor);

    /* Terminate BUF. */
    *p = 0;
    /* Reverse BUF. */
    p1 = buf;
    p2 = p - 1;
    while (p1 < p2) {
        char tmp = *p1;
        *p1 = *p2;
        *p2 = tmp;
        p1++;
        p2--;
    }
}

/* Put the character C on the screen. */
static void putchar (int c)
{
    if (c == '\n' || c == '\r') {
        newline:
        xpos = 0;
        ypos++;
        if (ypos >= LINES)
            ypos = 0;
        return;
    }

    *(video + (xpos + ypos * COLUMNS) * 2) = c & 0xFF;
    *(video + (xpos + ypos * COLUMNS) * 2 + 1) = ATTRIBUTE;

    xpos++;
    if (xpos >= COLUMNS)
        goto newline;
}

/* Format a string and print it on the screen, just like the libc
 * function printf. 
 */
void printf (const char *format, ...)
{
    char **arg = (char **) &format;
    int c;
    char buf[20];

    arg++;
    while ((c = *format++) != 0) {
        if (c != '%')
            putchar (c);
        else {
            char *p, *p2;
            int pad0 = 0, pad = 0;
            c = *format++;
            if (c == '0') {
                pad0 = 1;
                c = *format++;
            }
            if (c >= '0' && c <= '9') {
                pad = c - '0';
                c = *format++;
            }
            switch (c) {
                case 'd':
                case 'u':
                case 'x':
                    itoa (buf, c, *((int *) arg++));
                    p = buf;
                    goto string;
                    break;
                case 's':
                    p = *arg++;
                    if (! p)
                        p = "(null)";
                string:
                    for (p2 = p; *p2; p2++){}
                    for (; p2 < p + pad; p2++)
                        putchar (pad0 ? '0' : ' ');
                    while (*p)
                        putchar (*p++);
                    break;
                default:
                    putchar (*((int *) arg++));
                    break;
            }
        }
    }
}
