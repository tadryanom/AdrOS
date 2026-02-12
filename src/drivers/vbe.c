#include "vbe.h"
#include "vmm.h"
#include "pmm.h"
#include "devfs.h"
#include "fb.h"
#include "uaccess.h"
#include "uart_console.h"
#include "utils.h"

#include <stddef.h>

static struct vbe_info g_vbe;
static int g_vbe_ready = 0;
static fs_node_t g_dev_fb0_node;

int vbe_init(const struct boot_info* bi) {
    if (!bi || bi->fb_addr == 0 || bi->fb_width == 0 || bi->fb_height == 0 || bi->fb_bpp == 0) {
        uart_print("[VBE] No framebuffer provided by bootloader.\n");
        return -1;
    }

    g_vbe.phys_addr = bi->fb_addr;
    g_vbe.pitch = bi->fb_pitch;
    g_vbe.width = bi->fb_width;
    g_vbe.height = bi->fb_height;
    g_vbe.bpp = bi->fb_bpp;
    g_vbe.size = g_vbe.pitch * g_vbe.height;

    uint32_t pages = (g_vbe.size + 0xFFF) >> 12;
    uintptr_t virt_base = 0xE0000000U;

    for (uint32_t i = 0; i < pages; i++) {
        vmm_map_page((uint64_t)(g_vbe.phys_addr + i * 0x1000),
                     (uint64_t)(virt_base + i * 0x1000),
                     VMM_FLAG_PRESENT | VMM_FLAG_RW);
    }

    g_vbe.virt_addr = (volatile uint8_t*)virt_base;
    g_vbe_ready = 1;

    uart_print("[VBE] Framebuffer ");
    char buf[16];
    itoa(g_vbe.width, buf, 10); uart_print(buf);
    uart_print("x");
    itoa(g_vbe.height, buf, 10); uart_print(buf);
    uart_print("x");
    itoa(g_vbe.bpp, buf, 10); uart_print(buf);
    uart_print(" @ 0x");
    itoa_hex(g_vbe.phys_addr, buf); uart_print(buf);
    uart_print(" mapped to 0x");
    itoa_hex(virt_base, buf); uart_print(buf);
    uart_print("\n");

    return 0;
}

int vbe_available(void) {
    return g_vbe_ready;
}

const struct vbe_info* vbe_get_info(void) {
    if (!g_vbe_ready) return NULL;
    return &g_vbe;
}

void vbe_put_pixel(uint32_t x, uint32_t y, uint32_t color) {
    if (!g_vbe_ready) return;
    if (x >= g_vbe.width || y >= g_vbe.height) return;

    uint32_t offset = y * g_vbe.pitch + x * (g_vbe.bpp / 8);
    volatile uint8_t* pixel = g_vbe.virt_addr + offset;

    if (g_vbe.bpp == 32) {
        *(volatile uint32_t*)pixel = color;
    } else if (g_vbe.bpp == 24) {
        pixel[0] = (uint8_t)(color & 0xFF);
        pixel[1] = (uint8_t)((color >> 8) & 0xFF);
        pixel[2] = (uint8_t)((color >> 16) & 0xFF);
    } else if (g_vbe.bpp == 16) {
        *(volatile uint16_t*)pixel = (uint16_t)color;
    }
}

void vbe_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    if (!g_vbe_ready) return;

    uint32_t x_end = x + w;
    uint32_t y_end = y + h;
    if (x_end > g_vbe.width) x_end = g_vbe.width;
    if (y_end > g_vbe.height) y_end = g_vbe.height;

    uint32_t bytes_pp = g_vbe.bpp / 8;

    for (uint32_t row = y; row < y_end; row++) {
        volatile uint8_t* row_ptr = g_vbe.virt_addr + row * g_vbe.pitch + x * bytes_pp;
        if (g_vbe.bpp == 32) {
            volatile uint32_t* p = (volatile uint32_t*)row_ptr;
            for (uint32_t col = x; col < x_end; col++) {
                *p++ = color;
            }
        } else {
            for (uint32_t col = x; col < x_end; col++) {
                uint32_t off = (col - x) * bytes_pp;
                if (bytes_pp == 3) {
                    row_ptr[off]     = (uint8_t)(color & 0xFF);
                    row_ptr[off + 1] = (uint8_t)((color >> 8) & 0xFF);
                    row_ptr[off + 2] = (uint8_t)((color >> 16) & 0xFF);
                } else if (bytes_pp == 2) {
                    *(volatile uint16_t*)(row_ptr + off) = (uint16_t)color;
                }
            }
        }
    }
}

void vbe_clear(uint32_t color) {
    if (!g_vbe_ready) return;
    vbe_fill_rect(0, 0, g_vbe.width, g_vbe.height, color);
}

/* --- /dev/fb0 device callbacks --- */

static uint32_t fb0_read(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    (void)node;
    if (!g_vbe_ready || !buffer) return 0;
    if (offset >= g_vbe.size) return 0;
    uint32_t avail = g_vbe.size - offset;
    if (size > avail) size = avail;
    memcpy(buffer, (const uint8_t*)g_vbe.virt_addr + offset, size);
    return size;
}

static uint32_t fb0_write(fs_node_t* node, uint32_t offset, uint32_t size, const uint8_t* buffer) {
    (void)node;
    if (!g_vbe_ready || !buffer) return 0;
    if (offset >= g_vbe.size) return 0;
    uint32_t avail = g_vbe.size - offset;
    if (size > avail) size = avail;
    memcpy((uint8_t*)g_vbe.virt_addr + offset, buffer, size);
    return size;
}

static int fb0_ioctl(fs_node_t* node, uint32_t cmd, void* arg) {
    (void)node;
    if (!g_vbe_ready) return -1;
    if (!arg) return -1;

    if (cmd == FBIOGET_VSCREENINFO) {
        if (user_range_ok(arg, sizeof(struct fb_var_screeninfo)) == 0) return -1;
        struct fb_var_screeninfo v;
        v.xres = g_vbe.width;
        v.yres = g_vbe.height;
        v.bits_per_pixel = g_vbe.bpp;
        if (copy_to_user(arg, &v, sizeof(v)) < 0) return -1;
        return 0;
    }

    if (cmd == FBIOGET_FSCREENINFO) {
        if (user_range_ok(arg, sizeof(struct fb_fix_screeninfo)) == 0) return -1;
        struct fb_fix_screeninfo f;
        f.smem_start = (uint32_t)g_vbe.phys_addr;
        f.smem_len = g_vbe.size;
        f.line_length = g_vbe.pitch;
        if (copy_to_user(arg, &f, sizeof(f)) < 0) return -1;
        return 0;
    }

    return -1;
}

static uintptr_t fb0_mmap(fs_node_t* node, uintptr_t addr, uint32_t length, uint32_t prot, uint32_t offset) {
    (void)node; (void)prot; (void)offset;
    if (!g_vbe_ready) return 0;

    uint32_t aligned_len = (length + 0xFFFU) & ~(uint32_t)0xFFFU;
    if (aligned_len > ((g_vbe.size + 0xFFFU) & ~(uint32_t)0xFFFU))
        aligned_len = (g_vbe.size + 0xFFFU) & ~(uint32_t)0xFFFU;

    for (uint32_t i = 0; i < aligned_len; i += 0x1000U) {
        vmm_map_page((uint64_t)(g_vbe.phys_addr + i),
                     (uint64_t)(addr + i),
                     VMM_FLAG_PRESENT | VMM_FLAG_RW | VMM_FLAG_USER | VMM_FLAG_NOCACHE);
    }

    return addr;
}

void vbe_register_devfs(void) {
    if (!g_vbe_ready) return;

    memset(&g_dev_fb0_node, 0, sizeof(g_dev_fb0_node));
    strcpy(g_dev_fb0_node.name, "fb0");
    g_dev_fb0_node.flags = FS_CHARDEVICE;
    g_dev_fb0_node.inode = 20;
    g_dev_fb0_node.length = g_vbe.size;
    g_dev_fb0_node.read = &fb0_read;
    g_dev_fb0_node.write = &fb0_write;
    g_dev_fb0_node.ioctl = &fb0_ioctl;
    g_dev_fb0_node.mmap = &fb0_mmap;
    devfs_register_device(&g_dev_fb0_node);

    uart_print("[VBE] Registered /dev/fb0\n");
}
