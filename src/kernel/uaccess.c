#include "uaccess.h"

#include "errno.h"

#include <stdint.h>

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

static int x86_user_page_writable_user(uintptr_t vaddr) {
    volatile uint32_t* pd = (volatile uint32_t*)0xFFFFF000U;
    volatile uint32_t* pt_base = (volatile uint32_t*)0xFFC00000U;

    uint32_t pde = pd[vaddr >> 22];
    if (!(pde & 0x1)) return 0;
    if (!(pde & 0x4)) return 0;

    volatile uint32_t* pt = pt_base + ((vaddr >> 22) << 10);
    uint32_t pte = pt[(vaddr >> 12) & 0x3FF];
    if (!(pte & 0x1)) return 0;
    if (!(pte & 0x4)) return 0;
    if (!(pte & 0x2)) return 0;
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

static int x86_user_range_writable_user(uintptr_t uaddr, size_t len) {
    if (!x86_user_range_basic_ok(uaddr, len)) return 0;
    if (len == 0) return 1;

    uintptr_t start = uaddr & ~(uintptr_t)0xFFF;
    uintptr_t end = (uaddr + len - 1) & ~(uintptr_t)0xFFF;
    for (uintptr_t va = start;; va += 0x1000) {
        if (!x86_user_page_writable_user(va)) return 0;
        if (va == end) break;
    }
    return 1;
}
#endif

int user_range_ok(const void* user_ptr, size_t len) {
    uintptr_t uaddr = (uintptr_t)user_ptr;

#if defined(__i386__)
    return x86_user_range_mapped_and_user(uaddr, len);
#else
    if (len == 0) return 1;
    if (uaddr == 0) return 0;
    uintptr_t end = uaddr + len - 1;
    if (end < uaddr) return 0;
    return 1;
#endif
}

int copy_from_user(void* dst, const void* src_user, size_t len) {
    if (len == 0) return 0;
    if (!user_range_ok(src_user, len)) return -EFAULT;

    uintptr_t up = (uintptr_t)src_user;
    for (size_t i = 0; i < len; i++) {
        ((uint8_t*)dst)[i] = ((const volatile uint8_t*)up)[i];
    }
    return 0;
}

int copy_to_user(void* dst_user, const void* src, size_t len) {
    if (len == 0) return 0;

#if defined(__i386__)
    if (!x86_user_range_writable_user((uintptr_t)dst_user, len)) return -EFAULT;
#else
    if (!user_range_ok(dst_user, len)) return -EFAULT;
#endif

    uintptr_t up = (uintptr_t)dst_user;
    for (size_t i = 0; i < len; i++) {
        ((volatile uint8_t*)up)[i] = ((const uint8_t*)src)[i];
    }
    return 0;
}
