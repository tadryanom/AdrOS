#ifndef IO_H
#define IO_H

#include <stdint.h>

/* Generic MMIO helpers (ARM/RISC-V/MIPS and also usable on x86) */
static inline void mmio_write8(uintptr_t addr, uint8_t val) {
    volatile uint8_t* ptr = (uint8_t*)addr;
    *ptr = val;
}

static inline void mmio_write16(uintptr_t addr, uint16_t val) {
    volatile uint16_t* ptr = (uint16_t*)addr;
    *ptr = val;
}

static inline void mmio_write32(uintptr_t addr, uint32_t val) {
    volatile uint32_t* ptr = (uint32_t*)addr;
    *ptr = val;
}

static inline uint8_t mmio_read8(uintptr_t addr) {
    volatile uint8_t* ptr = (uint8_t*)addr;
    return *ptr;
}

static inline uint16_t mmio_read16(uintptr_t addr) {
    volatile uint16_t* ptr = (uint16_t*)addr;
    return *ptr;
}

static inline uint32_t mmio_read32(uintptr_t addr) {
    volatile uint32_t* ptr = (uint32_t*)addr;
    return *ptr;
}

/* x86 port I/O lives under include/arch/x86/io.h */
#if defined(__i386__) || defined(__x86_64__)
#include "arch/x86/io.h"
#endif

#endif
