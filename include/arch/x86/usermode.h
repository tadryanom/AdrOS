#ifndef ARCH_X86_USERMODE_H
#define ARCH_X86_USERMODE_H

#include <stdint.h>

#if defined(__i386__)
__attribute__((noreturn)) void x86_enter_usermode(uintptr_t user_eip, uintptr_t user_esp);
#endif

#endif
