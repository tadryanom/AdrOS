// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

/*
 * __libc_init_array / __libc_fini_array — run global constructors/destructors
 *
 * These iterate over .preinit_array, .init_array, and .fini_array sections
 * created by the linker.  Required for __attribute__((constructor)),
 * __attribute__((destructor)), and C++ static initializers.
 *
 * The symbols __preinit_array_start, __init_array_start, __init_array_end,
 * __fini_array_start, __fini_array_end are defined by the linker script.
 * _init() and _fini() come from crti.o/crtn.o (GCC CRT files).
 */

#include <stddef.h>

typedef void (*init_func)(void);

extern init_func __preinit_array_start[];
extern init_func __preinit_array_end[];
extern init_func __init_array_start[];
extern init_func __init_array_end[];
extern init_func __fini_array_start[];
extern init_func __fini_array_end[];

extern void _init(void);
extern void _fini(void);

void __libc_init_array(void) {
    size_t count, i;

    count = (size_t)(__preinit_array_end - __preinit_array_start);
    for (i = 0; i < count; i++)
        __preinit_array_start[i]();

    _init();

    count = (size_t)(__init_array_end - __init_array_start);
    for (i = 0; i < count; i++)
        __init_array_start[i]();
}

void __libc_fini_array(void) {
    size_t count;

    count = (size_t)(__fini_array_end - __fini_array_start);
    while (count-- > 0)
        __fini_array_start[count]();

    _fini();
}
