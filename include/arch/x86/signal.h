#ifndef ARCH_X86_SIGNAL_H
#define ARCH_X86_SIGNAL_H

#include <stdint.h>
#include "arch/x86/idt.h"

#define SIGFRAME_MAGIC 0x53494746U /* 'SIGF' */

struct sigframe {
    uint32_t magic;
    struct registers saved;
};

#endif /* ARCH_X86_SIGNAL_H */
