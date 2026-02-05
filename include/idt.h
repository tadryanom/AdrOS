#ifndef IDT_H
#define IDT_H

#include <stdint.h>

#if defined(__i386__) || defined(__x86_64__)
#include "arch/x86/idt.h"
#else

/* Non-x86: provide a minimal compatibility surface.
   Interrupt controller specifics live in arch code. */
struct registers {
    uint32_t int_no;
    uint32_t err_code;
};

typedef void (*isr_handler_t)(struct registers*);

static inline void idt_init(void) { }
static inline void register_interrupt_handler(uint8_t n, isr_handler_t handler) {
    (void)n;
    (void)handler;
}

#endif

#endif
