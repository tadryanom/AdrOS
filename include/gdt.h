#ifndef GDT_H
#define GDT_H

#if defined(__i386__) || defined(__x86_64__)
#include "arch/x86/gdt.h"
#else

#include <stdint.h>
#include <stddef.h>

static inline void gdt_init(void) { }
static inline void tss_set_kernel_stack(uintptr_t esp0) { (void)esp0; }

#endif

#endif
