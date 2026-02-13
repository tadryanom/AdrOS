#include "vga_console.h"

#include "hal/video.h"

#include "spinlock.h"

static volatile uint16_t* VGA_BUFFER = 0;
static const int VGA_WIDTH = 80;
static const int VGA_HEIGHT = 25;

static int term_col = 0;
static int term_row = 0;
static uint8_t term_color = 0x0F; // White on Black

static spinlock_t vga_lock = {0};

static void vga_scroll(void) {
    for (int y = 1; y < VGA_HEIGHT; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            VGA_BUFFER[(y - 1) * VGA_WIDTH + x] = VGA_BUFFER[y * VGA_WIDTH + x];
        }
    }
    for (int x = 0; x < VGA_WIDTH; x++) {
        VGA_BUFFER[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = (uint16_t)' ' | (uint16_t)term_color << 8;
    }
    term_row = VGA_HEIGHT - 1;
}

static void vga_update_hw_cursor(void) {
    hal_video_set_cursor(term_row, term_col);
}

static void vga_put_char_unlocked(char c) {
    if (!VGA_BUFFER) return;

    switch (c) {
    case '\n':
        term_col = 0;
        term_row++;
        break;
    case '\r':
        term_col = 0;
        break;
    case '\b':
        if (term_col > 0) {
            term_col--;
        } else if (term_row > 0) {
            term_row--;
            term_col = VGA_WIDTH - 1;
        }
        break;
    case '\t':
        term_col = (term_col + 8) & ~7;
        if (term_col >= VGA_WIDTH) {
            term_col = 0;
            term_row++;
        }
        break;
    default:
        if ((unsigned char)c >= ' ') {
            const int index = term_row * VGA_WIDTH + term_col;
            VGA_BUFFER[index] = (uint16_t)(unsigned char)c | (uint16_t)term_color << 8;
            term_col++;
        }
        break;
    }

    if (term_col >= VGA_WIDTH) {
        term_col = 0;
        term_row++;
    }

    if (term_row >= VGA_HEIGHT) {
        vga_scroll();
    }
}

void vga_init(void) {
    VGA_BUFFER = (volatile uint16_t*)hal_video_text_buffer();
    term_col = 0;
    term_row = 0;
    term_color = 0x07; // Light Grey on Black

    if (!VGA_BUFFER) {
        return;
    }
    
    for (int y = 0; y < VGA_HEIGHT; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            const int index = y * VGA_WIDTH + x;
            VGA_BUFFER[index] = (uint16_t) ' ' | (uint16_t) term_color << 8;
        }
    }
    vga_update_hw_cursor();
}

void vga_set_color(uint8_t fg, uint8_t bg) {
    uintptr_t flags = spin_lock_irqsave(&vga_lock);
    term_color = fg | (bg << 4);
    spin_unlock_irqrestore(&vga_lock, flags);
}

void vga_put_char(char c) {
    uintptr_t flags = spin_lock_irqsave(&vga_lock);
    vga_put_char_unlocked(c);
    vga_update_hw_cursor();
    spin_unlock_irqrestore(&vga_lock, flags);
}

void vga_print(const char* str) {
    uintptr_t flags = spin_lock_irqsave(&vga_lock);

    if (!VGA_BUFFER) {
        spin_unlock_irqrestore(&vga_lock, flags);
        return;
    }

    for (int i = 0; str[i] != '\0'; i++) {
        vga_put_char_unlocked(str[i]);
    }

    vga_update_hw_cursor();
    spin_unlock_irqrestore(&vga_lock, flags);
}

void vga_clear(void) {
    uintptr_t flags = spin_lock_irqsave(&vga_lock);

    if (!VGA_BUFFER) {
        spin_unlock_irqrestore(&vga_lock, flags);
        return;
    }

    for (int y = 0; y < VGA_HEIGHT; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            VGA_BUFFER[y * VGA_WIDTH + x] = (uint16_t)' ' | (uint16_t)term_color << 8;
        }
    }
    term_col = 0;
    term_row = 0;
    vga_update_hw_cursor();

    spin_unlock_irqrestore(&vga_lock, flags);
}
