#include "elf.h"

#include "fs.h"
#include "heap.h"
#include "pmm.h"
#include "console.h"
#include "utils.h"
#include "vmm.h"

#include "errno.h"

#include "hal/cpu.h"
#include "hal/mm.h"

#include <stdint.h>

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
    if (!eh) return -EFAULT;
    if (file_len < sizeof(*eh)) return -EINVAL;

    if (eh->e_ident[0] != ELF_MAGIC0 ||
        eh->e_ident[1] != ELF_MAGIC1 ||
        eh->e_ident[2] != ELF_MAGIC2 ||
        eh->e_ident[3] != ELF_MAGIC3) {
        return -EINVAL;
    }
    if (eh->e_ident[4] != ELFCLASS32) return -EINVAL;
    if (eh->e_ident[5] != ELFDATA2LSB) return -EINVAL;

    if (eh->e_type != ET_EXEC && eh->e_type != ET_DYN) return -EINVAL;
    if (eh->e_machine != EM_386) return -EINVAL;

    if (eh->e_phentsize != sizeof(elf32_phdr_t)) return -EINVAL;
    if (eh->e_phnum == 0) return -EINVAL;

    uint32_t ph_end = eh->e_phoff + (uint32_t)eh->e_phnum * (uint32_t)sizeof(elf32_phdr_t);
    if (ph_end < eh->e_phoff) return -EINVAL;
    if (ph_end > file_len) return -EINVAL;

    if (eh->e_entry == 0) return -EINVAL;
    if (eh->e_entry >= hal_mm_kernel_virt_base()) return -EINVAL;

    return 0;
}

static int elf32_map_user_range(uintptr_t as, uintptr_t vaddr, size_t len, uint32_t flags) {
    if (len == 0) return 0;
    if (vaddr == 0) return -EINVAL;
    if (vaddr >= hal_mm_kernel_virt_base()) return -EINVAL;

    uintptr_t end = vaddr + len - 1;
    if (end < vaddr) return -EINVAL;
    if (end >= hal_mm_kernel_virt_base()) return -EINVAL;

    uintptr_t start_page = vaddr & ~(uintptr_t)0xFFF;
    uintptr_t end_page = end & ~(uintptr_t)0xFFF;

    uintptr_t old_as = hal_cpu_get_address_space();
    vmm_as_activate(as);

    for (uintptr_t va = start_page;; va += 0x1000) {
        const uint32_t pi = (uint32_t)((va >> 30) & 0x3);
        const uint32_t di = (uint32_t)((va >> 21) & 0x1FF);
        const uint32_t ti = (uint32_t)((va >> 12) & 0x1FF);

        volatile uint64_t* pd = (volatile uint64_t*)(uintptr_t)(0xFFFFC000U + pi * 0x1000U);
        int already_mapped = 0;
        if ((pd[di] & 1ULL) != 0ULL) {
            volatile uint64_t* pt = (volatile uint64_t*)(uintptr_t)(0xFF800000U + pi * 0x200000U + di * 0x1000U);
            if ((pt[ti] & 1ULL) != 0ULL) {
                already_mapped = 1;
            }
        }

        if (!already_mapped) {
            void* phys = pmm_alloc_page_low_16mb();
            if (!phys) {
                vmm_as_activate(old_as);
                return -ENOMEM;
            }

            vmm_map_page((uint64_t)(uintptr_t)phys, (uint64_t)va, flags | VMM_FLAG_PRESENT | VMM_FLAG_USER);
        }

        if (va == end_page) break;
    }

    vmm_as_activate(old_as);

    return 0;
}

/* Load ELF segments at base_offset (0 for ET_EXEC, non-zero for interpreter).
 * Operates in the target address space (caller must have activated it).
 * Returns 0 on success, fills highest_end. */
static int elf32_load_segments(const uint8_t* file, uint32_t file_len,
                               uintptr_t as, uintptr_t base_offset,
                               uintptr_t* highest_end) {
    const elf32_ehdr_t* eh = (const elf32_ehdr_t*)file;
    const elf32_phdr_t* ph = (const elf32_phdr_t*)(file + eh->e_phoff);
    uintptr_t seg_max = 0;

    for (uint16_t i = 0; i < eh->e_phnum; i++) {
        if (ph[i].p_type != PT_LOAD) continue;
        if (ph[i].p_memsz == 0) continue;

        uintptr_t vaddr = (uintptr_t)ph[i].p_vaddr + base_offset;
        if (vaddr == 0 || vaddr >= hal_mm_kernel_virt_base()) return -EINVAL;

        uint32_t seg_end = (uint32_t)vaddr + ph[i].p_memsz;
        if (seg_end < vaddr || seg_end >= hal_mm_kernel_virt_base()) return -EINVAL;

        if ((uint64_t)ph[i].p_offset + (uint64_t)ph[i].p_filesz > (uint64_t)file_len)
            return -EINVAL;

        int mrc = elf32_map_user_range(as, vaddr, (size_t)ph[i].p_memsz, VMM_FLAG_RW);
        if (mrc < 0) return mrc;

        if (ph[i].p_filesz)
            memcpy((void*)vaddr, file + ph[i].p_offset, ph[i].p_filesz);
        if (ph[i].p_memsz > ph[i].p_filesz)
            memset((void*)(vaddr + ph[i].p_filesz), 0, ph[i].p_memsz - ph[i].p_filesz);

        if (seg_end > seg_max) seg_max = seg_end;
    }

    if (highest_end) *highest_end = seg_max;
    return 0;
}

/* Process ELF relocations from PT_DYNAMIC segment.
 * base_offset is 0 for ET_EXEC, non-zero for PIE/shared objects.
 * The target address space must already be activated. */
static void elf32_process_relocations(const uint8_t* file, uint32_t file_len,
                                       uintptr_t base_offset) {
    const elf32_ehdr_t* eh = (const elf32_ehdr_t*)file;
    const elf32_phdr_t* ph = (const elf32_phdr_t*)(file + eh->e_phoff);

    /* Find PT_DYNAMIC */
    const elf32_phdr_t* dyn_ph = NULL;
    for (uint16_t i = 0; i < eh->e_phnum; i++) {
        if (ph[i].p_type == PT_DYNAMIC) {
            dyn_ph = &ph[i];
            break;
        }
    }
    if (!dyn_ph) return;
    if (dyn_ph->p_offset + dyn_ph->p_filesz > file_len) return;

    /* Parse dynamic entries */
    const elf32_dyn_t* dyn = (const elf32_dyn_t*)(file + dyn_ph->p_offset);
    uint32_t dyn_count = dyn_ph->p_filesz / sizeof(elf32_dyn_t);

    uint32_t rel_addr = 0, rel_sz = 0;
    uint32_t jmprel_addr = 0, pltrelsz = 0;
    uint32_t symtab_addr = 0;

    for (uint32_t i = 0; i < dyn_count && dyn[i].d_tag != DT_NULL; i++) {
        switch (dyn[i].d_tag) {
        case DT_REL:     rel_addr = dyn[i].d_val; break;
        case DT_RELSZ:   rel_sz = dyn[i].d_val; break;
        case DT_JMPREL:  jmprel_addr = dyn[i].d_val; break;
        case DT_PLTRELSZ: pltrelsz = dyn[i].d_val; break;
        case DT_SYMTAB:  symtab_addr = dyn[i].d_val; break;
        }
    }

    /* Helper: apply a single relocation */
    #define APPLY_REL(rel_va, count) do { \
        for (uint32_t _r = 0; _r < (count); _r++) { \
            const elf32_rel_t* r = &((const elf32_rel_t*)(rel_va))[_r]; \
            uint32_t type = ELF32_R_TYPE(r->r_info); \
            uint32_t* target = (uint32_t*)(r->r_offset + base_offset); \
            if ((uintptr_t)target >= hal_mm_kernel_virt_base()) continue; \
            switch (type) { \
            case R_386_RELATIVE: \
                *target += (uint32_t)base_offset; \
                break; \
            case R_386_GLOB_DAT: \
            case R_386_JMP_SLOT: { \
                uint32_t sym_idx = ELF32_R_SYM(r->r_info); \
                if (symtab_addr && sym_idx) { \
                    const elf32_sym_t* sym = &((const elf32_sym_t*) \
                        (symtab_addr + base_offset))[sym_idx]; \
                    *target = sym->st_value + (uint32_t)base_offset; \
                } \
                break; \
            } \
            case R_386_32: { \
                uint32_t sym_idx = ELF32_R_SYM(r->r_info); \
                if (symtab_addr && sym_idx) { \
                    const elf32_sym_t* sym = &((const elf32_sym_t*) \
                        (symtab_addr + base_offset))[sym_idx]; \
                    *target += sym->st_value + (uint32_t)base_offset; \
                } \
                break; \
            } \
            default: break; \
            } \
        } \
    } while (0)

    /* Process .rel.dyn */
    if (rel_addr && rel_sz) {
        uint32_t va = rel_addr + (uint32_t)base_offset;
        uint32_t cnt = rel_sz / sizeof(elf32_rel_t);
        if (va < hal_mm_kernel_virt_base())
            APPLY_REL(va, cnt);
    }

    /* Process .rel.plt (JMPREL) */
    if (jmprel_addr && pltrelsz) {
        uint32_t va = jmprel_addr + (uint32_t)base_offset;
        uint32_t cnt = pltrelsz / sizeof(elf32_rel_t);
        if (va < hal_mm_kernel_virt_base())
            APPLY_REL(va, cnt);
    }

    #undef APPLY_REL
}

/* Load an interpreter ELF (ld.so) at INTERP_BASE.
 * Returns 0 on success, sets *interp_entry. */
#define INTERP_BASE 0x40000000U

static int elf32_load_interp(const char* interp_path, uintptr_t as,
                              uintptr_t* interp_entry, uintptr_t* interp_base_out) {
    if (!interp_path || !interp_entry) return -EINVAL;

    fs_node_t* node = vfs_lookup(interp_path);
    if (!node) {
        kprintf("[ELF] interp not found: %s\n", interp_path);
        return -ENOENT;
    }

    uint32_t flen = node->length;
    if (flen < sizeof(elf32_ehdr_t)) return -EINVAL;

    uint8_t* fbuf = (uint8_t*)kmalloc(flen);
    if (!fbuf) return -ENOMEM;

    if (vfs_read(node, 0, flen, fbuf) != flen) {
        kfree(fbuf);
        return -EIO;
    }

    const elf32_ehdr_t* eh = (const elf32_ehdr_t*)fbuf;
    int vrc = elf32_validate(eh, flen);
    if (vrc < 0) {
        kfree(fbuf);
        return vrc;
    }

    uintptr_t dummy = 0;
    int rc = elf32_load_segments(fbuf, flen, as, INTERP_BASE, &dummy);
    if (rc < 0) {
        kfree(fbuf);
        return rc;
    }

    *interp_entry = (uintptr_t)eh->e_entry + INTERP_BASE;
    if (interp_base_out) *interp_base_out = INTERP_BASE;

    kfree(fbuf);
    return 0;
}

int elf32_load_user_from_initrd(const char* filename, uintptr_t* entry_out, uintptr_t* user_stack_top_out, uintptr_t* addr_space_out, uintptr_t* heap_break_out) {
    if (!filename || !entry_out || !user_stack_top_out || !addr_space_out) return -EFAULT;
    if (!fs_root) return -EINVAL;

    uintptr_t new_as = vmm_as_create_kernel_clone();
    if (!new_as) return -ENOMEM;

    uintptr_t old_as = hal_cpu_get_address_space();

    fs_node_t* node = vfs_lookup(filename);
    if (!node) {
        kprintf("[ELF] file not found: %s\n", filename);
        vmm_as_destroy(new_as);
        return -ENOENT;
    }

    uint32_t file_len = node->length;
    if (file_len < sizeof(elf32_ehdr_t)) {
        vmm_as_activate(old_as);
        vmm_as_destroy(new_as);
        return -EINVAL;
    }

    uint8_t* file = (uint8_t*)kmalloc(file_len);
    if (!file) {
        vmm_as_destroy(new_as);
        return -ENOMEM;
    }

    uint32_t rd = vfs_read(node, 0, file_len, file);
    if (rd != file_len) {
        kfree(file);
        vmm_as_destroy(new_as);
        return -EIO;
    }

    vmm_as_activate(new_as);

    const elf32_ehdr_t* eh = (const elf32_ehdr_t*)file;
    int vrc = elf32_validate(eh, file_len);
    if (vrc < 0) {
        kprintf("[ELF] invalid ELF header\n");
        kfree(file);
        vmm_as_activate(old_as);
        vmm_as_destroy(new_as);
        return vrc;
    }

    uintptr_t highest_seg_end = 0;
    int lrc = elf32_load_segments(file, file_len, new_as, 0, &highest_seg_end);
    if (lrc < 0) {
        kprintf("[ELF] segment load failed\n");
        kfree(file);
        vmm_as_activate(old_as);
        vmm_as_destroy(new_as);
        return lrc;
    }

    /* Process relocations (R_386_RELATIVE, GLOB_DAT, JMP_SLOT, R_386_32) */
    elf32_process_relocations(file, file_len, 0);

    /* Check for PT_INTERP — if present, load the dynamic linker */
    const elf32_phdr_t* ph = (const elf32_phdr_t*)(file + eh->e_phoff);
    uintptr_t real_entry = (uintptr_t)eh->e_entry;
    int has_interp = 0;

    for (uint16_t i = 0; i < eh->e_phnum; i++) {
        if (ph[i].p_type == PT_INTERP) {
            if (ph[i].p_filesz == 0 || ph[i].p_filesz > 256) break;
            if (ph[i].p_offset + ph[i].p_filesz > file_len) break;

            char interp_path[256];
            memcpy(interp_path, file + ph[i].p_offset, ph[i].p_filesz);
            interp_path[ph[i].p_filesz - 1] = '\0';

            uintptr_t interp_entry = 0;
            uintptr_t interp_base = 0;
            int irc = elf32_load_interp(interp_path, new_as, &interp_entry, &interp_base);
            if (irc == 0) {
                real_entry = interp_entry;
                has_interp = 1;
                kprintf("[ELF] loaded interp: %s\n", interp_path);
            }
            break;
        }
    }
    (void)has_interp;

    /* 32 KB user stack with a 4 KB guard page below (unmapped).
     * Guard page at stack_base - 0x1000 is left unmapped so stack overflow
     * triggers a page fault → SIGSEGV instead of silent corruption.
     * ASLR: randomize stack base by up to 256 pages (1 MB). */
    extern uint32_t kaslr_offset(uint32_t max_pages);
    const uintptr_t aslr_stack_slide = kaslr_offset(256);
    const uintptr_t user_stack_base = 0x00800000U + aslr_stack_slide;
    const size_t user_stack_size = 0x8000;       /* 8 pages = 32 KB */

    int src2 = elf32_map_user_range(new_as, user_stack_base, user_stack_size, VMM_FLAG_RW);
    if (src2 < 0) {
        kprintf("[ELF] OOM mapping user stack\n");
        kfree(file);
        vmm_as_activate(old_as);
        vmm_as_destroy(new_as);
        return src2;
    }

    /* Map vDSO shared page read-only into user address space */
    {
        extern uintptr_t vdso_get_phys(void);
        uintptr_t vp = vdso_get_phys();
        if (vp) {
            vmm_map_page((uint64_t)vp, (uint64_t)0x007FE000U,
                         VMM_FLAG_PRESENT | VMM_FLAG_USER);
        }
    }

    *entry_out = real_entry;
    *user_stack_top_out = user_stack_base + user_stack_size;
    *addr_space_out = new_as;
    if (heap_break_out) {
        *heap_break_out = (highest_seg_end + 0xFFFU) & ~(uintptr_t)0xFFFU;
    }

    kfree(file);
    vmm_as_activate(old_as);
    return 0;
}
