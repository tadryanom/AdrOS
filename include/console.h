#ifndef CONSOLE_H
#define CONSOLE_H

#include <stddef.h>
#include <stdarg.h>

void console_init(void);
void console_enable_uart(int enabled);
void console_enable_vga(int enabled);

void console_write(const char* s);
void console_put_char(char c);

int kvsnprintf(char* out, size_t out_size, const char* fmt, va_list ap);
int ksnprintf(char* out, size_t out_size, const char* fmt, ...);

void kprintf(const char* fmt, ...);

int kgetc(void);

size_t klog_read(char* out, size_t out_size);

#endif
