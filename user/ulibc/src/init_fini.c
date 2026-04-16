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
 *
 * In a shared-library build these linker-generated symbols may remain
 * SHN_UNDEF (value 0) when the library has no .init_array / .fini_array
 * sections.  We guard against that by checking the GOT entries for NULL.
 */

#include <stddef.h>

typedef void (*init_func)(void);

/* These are resolved via the GOT in PIC mode; their values come from
 * GLOB_DAT relocations.  When the linker doesn't define them they
 * resolve to 0.  We read them through a volatile pointer so the
 * compiler cannot assume they are non-null. */
extern init_func *volatile __preinit_array_start[];
extern init_func *volatile __preinit_array_end[];
extern init_func *volatile __init_array_start[];
extern init_func *volatile __init_array_end[];
extern init_func *volatile __fini_array_start[];
extern init_func *volatile __fini_array_end[];

extern void _init(void);
extern void _fini(void);

/* Read _init/_fini through volatile to allow null check.
 * In PIC shared libs, these resolve via GOT and may be 0. */
static volatile init_func _init_ptr = _init;
static volatile init_func _fini_ptr = _fini;

void __libc_init_array(void) {
    size_t count, i;
    init_func *s, *e;

    s = (init_func*)__preinit_array_start;
    e = (init_func*)__preinit_array_end;
    if (s && e) {
        count = (size_t)(e - s);
        for (i = 0; i < count; i++)
            s[i]();
    }

    {
        init_func fn = _init_ptr;
        if (fn) fn();
    }

    s = (init_func*)__init_array_start;
    e = (init_func*)__init_array_end;
    if (s && e) {
        count = (size_t)(e - s);
        for (i = 0; i < count; i++)
            s[i]();
    }
}

void __libc_fini_array(void) {
    size_t count;
    init_func *s, *e;

    s = (init_func*)__fini_array_start;
    e = (init_func*)__fini_array_end;
    if (s && e) {
        count = (size_t)(e - s);
        while (count-- > 0)
            s[count]();
    }

    {
        init_func fn = _fini_ptr;
        if (fn) fn();
    }
}
