#include "elf.h"

#include "fs.h"
#include "heap.h"
#include "pmm.h"
#include "uart_console.h"
#include "utils.h"
#include "vmm.h"

#include <stdint.h>

#if defined(__i386__)
#define X86_KERNEL_VIRT_BASE 0xC0000000U

static void* pmm_alloc_page_low_16mb(void) {
    for (int tries = 0; tries < 4096; tries++) {
        void* p = pmm_alloc_page();
        if (!p) return NULL;
        if ((uintptr_t)p < 0x01000000U) {
            return p;
        }
        pmm_free_page(p);
    }
    return NULL;
}

static int elf32_validate(const elf32_ehdr_t* eh, size_t file_len) {
    if (!eh) return -1;
    if (file_len < sizeof(*eh)) return -1;

    if (eh->e_ident[0] != ELF_MAGIC0 ||
        eh->e_ident[1] != ELF_MAGIC1 ||
        eh->e_ident[2] != ELF_MAGIC2 ||
        eh->e_ident[3] != ELF_MAGIC3) {
        return -1;
    }
    if (eh->e_ident[4] != ELFCLASS32) return -1;
    if (eh->e_ident[5] != ELFDATA2LSB) return -1;

    if (eh->e_type != ET_EXEC) return -1;
    if (eh->e_machine != EM_386) return -1;

    if (eh->e_phentsize != sizeof(elf32_phdr_t)) return -1;
    if (eh->e_phnum == 0) return -1;

    uint32_t ph_end = eh->e_phoff + (uint32_t)eh->e_phnum * (uint32_t)sizeof(elf32_phdr_t);
    if (ph_end < eh->e_phoff) return -1;
    if (ph_end > file_len) return -1;

    if (eh->e_entry == 0) return -1;
    if (eh->e_entry >= X86_KERNEL_VIRT_BASE) return -1;

    return 0;
}

static int elf32_map_user_range(uintptr_t vaddr, size_t len, uint32_t flags) {
    if (len == 0) return 0;
    if (vaddr == 0) return -1;
    if (vaddr >= X86_KERNEL_VIRT_BASE) return -1;

    uintptr_t end = vaddr + len - 1;
    if (end < vaddr) return -1;
    if (end >= X86_KERNEL_VIRT_BASE) return -1;

    uintptr_t start_page = vaddr & ~(uintptr_t)0xFFF;
    uintptr_t end_page = end & ~(uintptr_t)0xFFF;

    for (uintptr_t va = start_page;; va += 0x1000) {
        void* phys = pmm_alloc_page_low_16mb();
        if (!phys) return -1;

        vmm_map_page((uint64_t)(uintptr_t)phys, (uint64_t)va, flags | VMM_FLAG_PRESENT | VMM_FLAG_USER);

        if (va == end_page) break;
    }

    return 0;
}

int elf32_load_user_from_initrd(const char* filename, uintptr_t* entry_out, uintptr_t* user_stack_top_out) {
    if (!filename || !entry_out || !user_stack_top_out) return -1;
    if (!fs_root) return -1;

    fs_node_t* node = vfs_lookup(filename);
    if (!node) {
        uart_print("[ELF] file not found: ");
        uart_print(filename);
        uart_print("\n");
        return -1;
    }

    uint32_t file_len = node->length;
    if (file_len < sizeof(elf32_ehdr_t)) return -1;

    uint8_t* file = (uint8_t*)kmalloc(file_len);
    if (!file) return -1;

    uint32_t rd = vfs_read(node, 0, file_len, file);
    if (rd != file_len) {
        kfree(file);
        return -1;
    }

    const elf32_ehdr_t* eh = (const elf32_ehdr_t*)file;
    if (elf32_validate(eh, file_len) < 0) {
        uart_print("[ELF] invalid ELF header\n");
        kfree(file);
        return -1;
    }

    const elf32_phdr_t* ph = (const elf32_phdr_t*)(file + eh->e_phoff);

    for (uint16_t i = 0; i < eh->e_phnum; i++) {
        if (ph[i].p_type != PT_LOAD) continue;

        if (ph[i].p_memsz == 0) continue;
        if (ph[i].p_vaddr == 0) {
            uart_print("[ELF] PT_LOAD with vaddr=0 rejected\n");
            kfree(file);
            return -1;
        }
        if (ph[i].p_vaddr >= X86_KERNEL_VIRT_BASE) {
            uart_print("[ELF] PT_LOAD in kernel range rejected\n");
            kfree(file);
            return -1;
        }

        uint32_t seg_end = ph[i].p_vaddr + ph[i].p_memsz;
        if (seg_end < ph[i].p_vaddr) {
            kfree(file);
            return -1;
        }
        if (seg_end >= X86_KERNEL_VIRT_BASE) {
            kfree(file);
            return -1;
        }

        if ((uint64_t)ph[i].p_offset + (uint64_t)ph[i].p_filesz > (uint64_t)file_len) {
            uart_print("[ELF] segment outside file\n");
            kfree(file);
            return -1;
        }

        const uint32_t map_flags = VMM_FLAG_RW;

        if (elf32_map_user_range((uintptr_t)ph[i].p_vaddr, (size_t)ph[i].p_memsz, map_flags) < 0) {
            uart_print("[ELF] OOM mapping user segment\n");
            kfree(file);
            return -1;
        }

        if (ph[i].p_filesz) {
            memcpy((void*)(uintptr_t)ph[i].p_vaddr, file + ph[i].p_offset, ph[i].p_filesz);
        }

        if (ph[i].p_memsz > ph[i].p_filesz) {
            memset((void*)(uintptr_t)(ph[i].p_vaddr + ph[i].p_filesz), 0, ph[i].p_memsz - ph[i].p_filesz);
        }

        if ((ph[i].p_flags & PF_W) == 0) {
            vmm_protect_range((uint64_t)(uintptr_t)ph[i].p_vaddr, (uint64_t)ph[i].p_memsz,
                              VMM_FLAG_USER);
        }
    }

    const uintptr_t user_stack_base = 0x00800000U;
    const size_t user_stack_size = 0x1000;

    if (elf32_map_user_range(user_stack_base, user_stack_size, VMM_FLAG_RW) < 0) {
        uart_print("[ELF] OOM mapping user stack\n");
        kfree(file);
        return -1;
    }

    *entry_out = (uintptr_t)eh->e_entry;
    *user_stack_top_out = user_stack_base + user_stack_size;

    kfree(file);
    return 0;
}
#else
int elf32_load_user_from_initrd(const char* filename, uintptr_t* entry_out, uintptr_t* user_stack_top_out) {
    (void)filename;
    (void)entry_out;
    (void)user_stack_top_out;
    return -1;
}
#endif
