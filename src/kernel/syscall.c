#include "syscall.h"
#include "idt.h"
#include "uart_console.h"

#include <stddef.h>

#if defined(__i386__)
#define X86_KERNEL_VIRT_BASE 0xC0000000U

static int x86_user_range_basic_ok(uintptr_t uaddr, size_t len) {
    if (len == 0) return 1;
    if (uaddr == 0) return 0;
    if (uaddr >= X86_KERNEL_VIRT_BASE) return 0;
    uintptr_t end = uaddr + len - 1;
    if (end < uaddr) return 0;
    if (end >= X86_KERNEL_VIRT_BASE) return 0;
    return 1;
}

static int x86_user_page_present_and_user(uintptr_t vaddr) {
    volatile uint32_t* pd = (volatile uint32_t*)0xFFFFF000U;
    volatile uint32_t* pt_base = (volatile uint32_t*)0xFFC00000U;

    uint32_t pde = pd[vaddr >> 22];
    if (!(pde & 0x1)) return 0;
    if (!(pde & 0x4)) return 0;

    volatile uint32_t* pt = pt_base + ((vaddr >> 22) << 10);
    uint32_t pte = pt[(vaddr >> 12) & 0x3FF];
    if (!(pte & 0x1)) return 0;
    if (!(pte & 0x4)) return 0;

    return 1;
}

static int x86_user_range_mapped_and_user(uintptr_t uaddr, size_t len) {
    if (!x86_user_range_basic_ok(uaddr, len)) return 0;
    if (len == 0) return 1;

    uintptr_t start = uaddr & ~(uintptr_t)0xFFF;
    uintptr_t end = (uaddr + len - 1) & ~(uintptr_t)0xFFF;
    for (uintptr_t va = start;; va += 0x1000) {
        if (!x86_user_page_present_and_user(va)) return 0;
        if (va == end) break;
    }
    return 1;
}
#endif

static void syscall_handler(struct registers* regs) {
    uint32_t syscall_no = regs->eax;

    if (syscall_no == SYSCALL_WRITE) {
        uint32_t fd = regs->ebx;
        const char* buf = (const char*)regs->ecx;
        uint32_t len = regs->edx;

        if (fd != 1 && fd != 2) {
            regs->eax = (uint32_t)-1;
            return;
        }

        if (len > 1024 * 1024) {
            regs->eax = (uint32_t)-1;
            return;
        }

#if defined(__i386__)
        if (!x86_user_range_mapped_and_user((uintptr_t)buf, (size_t)len)) {
            regs->eax = (uint32_t)-1;
            return;
        }
#endif

        char kbuf[256];
        uint32_t remaining = len;
        uintptr_t up = (uintptr_t)buf;

        while (remaining) {
            uint32_t chunk = remaining;
            if (chunk > sizeof(kbuf)) chunk = (uint32_t)sizeof(kbuf);

#if defined(__i386__)
            if (!x86_user_range_mapped_and_user(up, (size_t)chunk)) {
                regs->eax = (uint32_t)-1;
                return;
            }
#endif

            for (uint32_t i = 0; i < chunk; i++) {
                kbuf[i] = ((const volatile char*)up)[i];
            }

            for (uint32_t i = 0; i < chunk; i++) {
                uart_put_char(kbuf[i]);
            }

            up += chunk;
            remaining -= chunk;
        }

        regs->eax = len;
        return;
    }

    if (syscall_no == SYSCALL_GETPID) {
        regs->eax = 0;
        return;
    }

    if (syscall_no == SYSCALL_EXIT) {
        uart_print("[USER] exit()\n");
        for(;;) {
            __asm__ volatile("cli; hlt");
        }
    }

    regs->eax = (uint32_t)-1;
}

void syscall_init(void) {
#if defined(__i386__)
    register_interrupt_handler(128, syscall_handler);
#endif
}
