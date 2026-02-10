#ifndef ARCH_X86_GDT_H
#define ARCH_X86_GDT_H

#include <stdint.h>
#include <stddef.h>

struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

extern struct gdt_ptr gp;

void gdt_init(void);
void tss_set_kernel_stack(uintptr_t esp0);

#endif
