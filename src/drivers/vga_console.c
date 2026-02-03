#include "vga_console.h"

static volatile uint16_t* const VGA_BUFFER = (uint16_t*) 0xB8000;
static const int VGA_WIDTH = 80;
static const int VGA_HEIGHT = 25;

static int term_col = 0;
static int term_row = 0;
static uint8_t term_color = 0x0F; // White on Black

void vga_init(void) {
    term_col = 0;
    term_row = 0;
    term_color = 0x07; // Light Grey on Black
    
    for (int y = 0; y < VGA_HEIGHT; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            const int index = y * VGA_WIDTH + x;
            VGA_BUFFER[index] = (uint16_t) ' ' | (uint16_t) term_color << 8;
        }
    }
}

void vga_set_color(uint8_t fg, uint8_t bg) {
    term_color = fg | (bg << 4);
}

void vga_put_char(char c) {
    if (c == '\n') {
        term_col = 0;
        term_row++;
    } else {
        const int index = term_row * VGA_WIDTH + term_col;
        VGA_BUFFER[index] = (uint16_t) c | (uint16_t) term_color << 8;
        term_col++;
    }

    if (term_col >= VGA_WIDTH) {
        term_col = 0;
        term_row++;
    }

    if (term_row >= VGA_HEIGHT) {
        // TODO: Implement scrolling
        term_row = 0;
    }
}

void vga_print(const char* str) {
    for (int i = 0; str[i] != '\0'; i++) {
        vga_put_char(str[i]);
    }
}
