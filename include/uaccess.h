#ifndef UACCESS_H
#define UACCESS_H

#include <stddef.h>

int user_range_ok(const void* user_ptr, size_t len);
int copy_from_user(void* dst, const void* src_user, size_t len);
int copy_to_user(void* dst_user, const void* src, size_t len);

#endif
