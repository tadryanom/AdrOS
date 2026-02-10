#include "uaccess.h"

#include "errno.h"
#include "interrupts.h"

#include <stdint.h>
#include <stddef.h>

__attribute__((weak))
int uaccess_try_recover(uintptr_t fault_addr, struct registers* regs) {
    (void)fault_addr;
    (void)regs;
    return 0;
}

__attribute__((weak))
int user_range_ok(const void* user_ptr, size_t len) {
    uintptr_t uaddr = (uintptr_t)user_ptr;
    if (len == 0) return 1;
    if (uaddr == 0) return 0;
    uintptr_t end = uaddr + len - 1;
    if (end < uaddr) return 0;
    return 1;
}

__attribute__((weak))
int copy_from_user(void* dst, const void* src_user, size_t len) {
    if (len == 0) return 0;
    if (!user_range_ok(src_user, len)) return -EFAULT;

    const uint8_t* s = (const uint8_t*)src_user;
    uint8_t* d = (uint8_t*)dst;
    for (size_t i = 0; i < len; i++) {
        d[i] = s[i];
    }
    return 0;
}

__attribute__((weak))
int copy_to_user(void* dst_user, const void* src, size_t len) {
    if (len == 0) return 0;
    if (!user_range_ok(dst_user, len)) return -EFAULT;

    uint8_t* d = (uint8_t*)dst_user;
    const uint8_t* s = (const uint8_t*)src;
    for (size_t i = 0; i < len; i++) {
        d[i] = s[i];
    }
    return 0;
}
