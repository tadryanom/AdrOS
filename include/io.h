#ifndef IO_H
#define IO_H

#include <stdint.h>

/* x86 I/O Port Wrappers */
#if defined(__i386__) || defined(__x86_64__)

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void mmio_write8(uintptr_t addr, uint8_t val) {
    volatile uint8_t* ptr = (uint8_t*)addr;
    *ptr = val;
}

static inline uint8_t mmio_read8(uintptr_t addr) {
    volatile uint8_t* ptr = (uint8_t*)addr;
    return *ptr;
}

#else

/* MMIO for ARM/RISC-V */
static inline void mmio_write8(uintptr_t addr, uint8_t val) {
    volatile uint8_t* ptr = (uint8_t*)addr;
    *ptr = val;
}

static inline uint8_t mmio_read8(uintptr_t addr) {
    volatile uint8_t* ptr = (uint8_t*)addr;
    return *ptr;
}

/* Fallback for port I/O on architectures that don't have it (mapped to MMIO or no-op) */
static inline void outb(uint16_t port, uint8_t val) {
    (void)port; (void)val; // No-op
}

static inline uint8_t inb(uint16_t port) {
    (void)port; return 0;
}

#endif

#endif
