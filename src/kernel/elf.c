#include "elf.h"
#include <stdint.h>

__attribute__((weak))
int elf32_load_user_from_initrd(const char* filename, uintptr_t* entry_out, uintptr_t* user_stack_top_out, uintptr_t* addr_space_out, uintptr_t* heap_break_out) {
    (void)filename;
    (void)entry_out;
    (void)user_stack_top_out;
    (void)addr_space_out;
    (void)heap_break_out;
    return -1;
}
