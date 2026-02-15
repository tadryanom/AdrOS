/*
 * ARM64 stub implementations for kernel subsystems not yet ported.
 * Provides weak symbols so main.c and console.c link successfully.
 */
#include <stdint.h>
#include <stddef.h>
#include "hal/uart.h"
#include "spinlock.h"

/* ---- UART console (wraps HAL UART) ---- */
static spinlock_t uart_lock = {0};

void uart_init(void) {
    hal_uart_init();
}

void uart_put_char(char c) {
    uintptr_t flags = spin_lock_irqsave(&uart_lock);
    hal_uart_putc(c);
    spin_unlock_irqrestore(&uart_lock, flags);
}

void uart_print(const char* str) {
    uintptr_t flags = spin_lock_irqsave(&uart_lock);
    for (int i = 0; str[i] != '\0'; i++)
        hal_uart_putc(str[i]);
    spin_unlock_irqrestore(&uart_lock, flags);
}

/* ---- VGA console (no-op on ARM) ---- */
void vga_init(void) { }
void vga_put_char(char c) { (void)c; }
void vga_write_buf(const char* buf, uint32_t len) { (void)buf; (void)len; }
void vga_print(const char* str) { (void)str; }
void vga_set_color(uint8_t fg, uint8_t bg) { (void)fg; (void)bg; }
void vga_flush(void) { }
void vga_clear(void) { }
void vga_scroll_back(void) { }
void vga_scroll_fwd(void) { }

/* ---- Kernel subsystem stubs (not yet ported) ---- */
void pmm_init(void* mboot_info) { (void)mboot_info; }
void kheap_init(void) { }
void shm_init(void) { }
void kaslr_init(void) { }
void process_init(void) { }
void vdso_init(void) { }
void timer_init(uint32_t hz) { (void)hz; }
int  init_start(const void* bi) { (void)bi; return -1; }
void kconsole_enter(void) { }

/* ---- Keyboard (no-op) ---- */
void keyboard_init(void) { }
int  keyboard_getchar(void) { return -1; }
int  keyboard_read_nonblock(void) { return -1; }

/* ---- HAL CPU extras ---- */
void hal_cpu_set_address_space(uintptr_t as) { (void)as; }
void hal_cpu_disable_interrupts(void) {
    __asm__ volatile("msr daifset, #2" ::: "memory");
}
uint64_t hal_cpu_read_timestamp(void) { return 0; }
void hal_cpu_set_tls(uintptr_t base) { (void)base; }
