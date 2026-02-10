#include "vbe.h"
#include "vmm.h"
#include "uart_console.h"
#include "utils.h"

#include <stddef.h>

static struct vbe_info g_vbe;
static int g_vbe_ready = 0;

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
    uintptr_t virt_base = 0xD0000000U;

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
