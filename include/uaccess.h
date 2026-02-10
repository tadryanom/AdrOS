#ifndef UACCESS_H
#define UACCESS_H

#include <stddef.h>
#include <stdint.h>

struct registers;

int user_range_ok(const void* user_ptr, size_t len);
int copy_from_user(void* dst, const void* src_user, size_t len);
int copy_to_user(void* dst_user, const void* src, size_t len);

int uaccess_try_recover(uintptr_t fault_addr, struct registers* regs);

#endif
