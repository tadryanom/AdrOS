#include <stdint.h>
#include "uart_console.h"
#include "io.h"

/* 
 * Hardware Constants 
 * These should ideally be in a platform-specific config or Device Tree.
 * For now, we hardcode for QEMU 'virt' and x86 standard PC.
 */

#if defined(__i386__) || defined(__x86_64__)
    #define UART_IS_PORT_IO 1
    #define UART_BASE 0x3F8  // COM1
#elif defined(__aarch64__)
    #define UART_IS_PORT_IO 0
    #define UART_BASE 0x09000000 // PL011 on QEMU virt
#elif defined(__riscv)
    #define UART_IS_PORT_IO 0
    #define UART_BASE 0x10000000 // NS16550A on QEMU virt
#else
    #define UART_IS_PORT_IO 0
    #define UART_BASE 0 // Unknown
#endif

void uart_init(void) {
#if defined(__i386__) || defined(__x86_64__)
    outb(UART_BASE + 1, 0x00);    // Disable all interrupts
    outb(UART_BASE + 3, 0x80);    // Enable DLAB (set baud rate divisor)
    outb(UART_BASE + 0, 0x03);    // Set divisor to 3 (lo byte) 38400 baud
    outb(UART_BASE + 1, 0x00);    //                  (hi byte)
    outb(UART_BASE + 3, 0x03);    // 8 bits, no parity, one stop bit
    outb(UART_BASE + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
    outb(UART_BASE + 4, 0x0B);    // IRQs enabled, RTS/DSR set
#elif defined(__riscv)
    // NS16550A setup (minimal)
    mmio_write8(UART_BASE + 3, 0x03); // LCR: 8-bit, no parity
    mmio_write8(UART_BASE + 2, 0x01); // FCR: Enable FIFO
    mmio_write8(UART_BASE + 1, 0x01); // IER: Enable TX interrupts (optional)
#elif defined(__aarch64__)
    // PL011 setup (simplified, assumes firmware initialized clocks)
    // Actually PL011 is different register map than 16550.
    // Base registers are 32-bit aligned usually.
    // For now, relies on QEMU default state or minimal write.
    // (Real implementation requires full PL011 driver logic)
#endif
}

void uart_put_char(char c) {
#if defined(__aarch64__)
    // PL011 Register map
    // DR (Data Register) is at offset 0x00
    // FR (Flag Register) is at offset 0x18. Bit 5 is TXFF (Transmit FIFO Full).
    volatile uint32_t* uart = (volatile uint32_t*)UART_BASE;
    while (uart[6] & (1 << 5)) { ; } // Wait while TXFF is set
    uart[0] = c;
#else
    // 16550 / x86 logic
    if (UART_IS_PORT_IO) {
        while ((inb(UART_BASE + 5) & 0x20) == 0); // Wait for empty transmit holding register
        outb(UART_BASE, c);
    } else {
        while ((mmio_read8(UART_BASE + 5) & 0x20) == 0);
        mmio_write8(UART_BASE, c);
    }
#endif
}

void uart_print(const char* str) {
    for (int i = 0; str[i] != '\0'; i++) {
        uart_put_char(str[i]);
    }
}
