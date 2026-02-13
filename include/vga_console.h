#ifndef VGA_CONSOLE_H
#define VGA_CONSOLE_H

#include <stdint.h>

void vga_init(void);
void vga_put_char(char c);
void vga_print(const char* str);
void vga_set_color(uint8_t fg, uint8_t bg);
void vga_clear(void);
void vga_scroll_back(void);
void vga_scroll_fwd(void);

#endif
