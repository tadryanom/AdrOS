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

/* Pending auxv buffer — filled by elf32_load_user_from_initrd when an
 * interpreter is present, consumed by execve to push onto the user stack
 * in the correct position (right after envp[]). */
static elf32_auxv_t g_pending_auxv[8];
static int          g_pending_auxv_count = 0;

int elf32_pop_pending_auxv(elf32_auxv_t* out, int max) {
    int n = g_pending_auxv_count;
    if (n == 0) return 0;
    if (n > max) n = max;
    for (int i = 0; i < n; i++) out[i] = g_pending_auxv[i];
    g_pending_auxv_count = 0;
    return n;
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

    if (eh->e_entry != 0 && eh->e_entry >= hal_mm_kernel_virt_base()) return -EINVAL;

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
            void* phys = pmm_alloc_page();
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
 * skip_jmpslot: if true, skip R_386_JMP_SLOT (let ld.so handle lazily).
 * The target address space must already be activated. */
static void elf32_process_relocations(const uint8_t* file, uint32_t file_len,
                                       uintptr_t base_offset, int skip_jmpslot) {
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
            case R_386_JMP_SLOT: \
                if (skip_jmpslot) break; \
                /* fall through */ \
            case R_386_GLOB_DAT: { \
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

/* Load a shared library ELF at the given base VA.
 * Returns 0 on success, fills *loaded_end with highest mapped address. */
static int elf32_load_shared_lib_at(const char* path, uintptr_t as,
                                     uintptr_t base, uintptr_t* loaded_end) {
    fs_node_t* node = vfs_lookup(path);
    if (!node) return -ENOENT;

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
    if (vrc < 0) { kfree(fbuf); return vrc; }

    uintptr_t seg_end = 0;
    int rc = elf32_load_segments(fbuf, flen, as, base, &seg_end);
    if (rc < 0) { kfree(fbuf); return rc; }

    elf32_process_relocations(fbuf, flen, base, 0);

    if (loaded_end) *loaded_end = seg_end;
    kfree(fbuf);
    return 0;
}

/* Load DT_NEEDED shared libraries from the main binary's PT_DYNAMIC.
 * Libraries are loaded sequentially starting at *next_base.
 * Returns number of libraries loaded. */
#define SHLIB_BASE 0x11000000U

static int elf32_load_needed_libs(const uint8_t* file, uint32_t file_len,
                                   uintptr_t as, uintptr_t base_offset) {
    const elf32_ehdr_t* eh = (const elf32_ehdr_t*)file;
    const elf32_phdr_t* ph = (const elf32_phdr_t*)(file + eh->e_phoff);

    const elf32_phdr_t* dyn_ph = NULL;
    for (uint16_t i = 0; i < eh->e_phnum; i++) {
        if (ph[i].p_type == PT_DYNAMIC) { dyn_ph = &ph[i]; break; }
    }
    if (!dyn_ph) return 0;
    if (dyn_ph->p_offset + dyn_ph->p_filesz > file_len) return 0;

    const elf32_dyn_t* dyn = (const elf32_dyn_t*)(file + dyn_ph->p_offset);
    uint32_t dyn_count = dyn_ph->p_filesz / sizeof(elf32_dyn_t);

    uint32_t strtab_addr = 0;
    for (uint32_t i = 0; i < dyn_count && dyn[i].d_tag != DT_NULL; i++) {
        if (dyn[i].d_tag == DT_STRTAB) { strtab_addr = dyn[i].d_val; break; }
    }
    if (!strtab_addr) return 0;

    const char* strtab = (const char*)(strtab_addr + base_offset);
    uintptr_t lib_base = SHLIB_BASE;
    int loaded = 0;

    for (uint32_t i = 0; i < dyn_count && dyn[i].d_tag != DT_NULL; i++) {
        if (dyn[i].d_tag != DT_NEEDED) continue;
        const char* libname = strtab + dyn[i].d_val;

        char path[128];
        int plen = 0;
        const char* pfx = "/lib/";
        while (*pfx && plen < 122) path[plen++] = *pfx++;
        const char* s = libname;
        while (*s && plen < 127) path[plen++] = *s++;
        path[plen] = '\0';

        uintptr_t seg_end = 0;
        int rc = elf32_load_shared_lib_at(path, as, lib_base, &seg_end);
        if (rc == 0) {
            /* shared lib loaded silently */
            lib_base = (seg_end + 0xFFFU) & ~(uintptr_t)0xFFFU;
            loaded++;
        } else {
            kprintf("[ELF] warning: could not load %s (%d)\n", path, rc);
        }
    }
    return loaded;
}

/* Load an interpreter ELF (ld.so) at INTERP_BASE.
 * Returns 0 on success, sets *interp_entry. */
#define INTERP_BASE 0x12000000U

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

    /* ET_EXEC interpreter has absolute addresses (no offset needed).
     * ET_DYN interpreter is position-independent, loaded at INTERP_BASE. */
    uintptr_t base_off = (eh->e_type == ET_DYN) ? INTERP_BASE : 0;

    uintptr_t dummy = 0;
    int rc = elf32_load_segments(fbuf, flen, as, base_off, &dummy);
    if (rc < 0) {
        kfree(fbuf);
        return rc;
    }

    if (eh->e_type == ET_DYN) {
        elf32_process_relocations(fbuf, flen, base_off, 0);
    }

    *interp_entry = (uintptr_t)eh->e_entry + base_off;
    if (interp_base_out) *interp_base_out = (base_off ? base_off : (uintptr_t)eh->e_entry);

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
    if (vrc < 0 || eh->e_entry == 0) {
        kprintf("[ELF] invalid ELF header\n");
        kfree(file);
        vmm_as_activate(old_as);
        vmm_as_destroy(new_as);
        return vrc < 0 ? vrc : -EINVAL;
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

    /* Check for PT_INTERP first — determines relocation strategy */
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
                /* interp loaded silently */
            }
            break;
        }
    }

    /* Process relocations — skip JMP_SLOT when ld.so will handle them lazily */
    elf32_process_relocations(file, file_len, 0, has_interp);

    /* Load DT_NEEDED shared libraries (kernel loads segments, ld.so resolves PLT) */
    if (has_interp) {
        elf32_load_needed_libs(file, file_len, new_as, 0);
    }
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

    uintptr_t sp = user_stack_base + user_stack_size;

    /* When an interpreter is loaded, save auxv entries into a static buffer.
     * The execve handler will push them onto the user stack in the correct
     * position (right after envp[]) so ld.so can find them. */
    if (has_interp) {
        /* Compute AT_PHDR: find the first PT_LOAD that covers e_phoff */
        uint32_t phdr_va = 0;
        for (uint16_t i = 0; i < eh->e_phnum; i++) {
            if (ph[i].p_type == PT_LOAD &&
                eh->e_phoff >= ph[i].p_offset &&
                eh->e_phoff < ph[i].p_offset + ph[i].p_filesz) {
                phdr_va = ph[i].p_vaddr + (eh->e_phoff - ph[i].p_offset);
                break;
            }
        }
        g_pending_auxv[0].a_type = AT_ENTRY;  g_pending_auxv[0].a_val = (uint32_t)eh->e_entry;
        g_pending_auxv[1].a_type = AT_BASE;   g_pending_auxv[1].a_val = INTERP_BASE;
        g_pending_auxv[2].a_type = AT_PAGESZ; g_pending_auxv[2].a_val = 0x1000;
        g_pending_auxv[3].a_type = AT_PHDR;   g_pending_auxv[3].a_val = phdr_va;
        g_pending_auxv[4].a_type = AT_PHNUM;  g_pending_auxv[4].a_val = eh->e_phnum;
        g_pending_auxv[5].a_type = AT_PHENT;  g_pending_auxv[5].a_val = eh->e_phentsize;
        g_pending_auxv[6].a_type = AT_NULL;   g_pending_auxv[6].a_val = 0;
        g_pending_auxv_count = 7;
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
    *user_stack_top_out = sp;
    *addr_space_out = new_as;
    if (heap_break_out) {
        *heap_break_out = (highest_seg_end + 0xFFFU) & ~(uintptr_t)0xFFFU;
    }

    kfree(file);
    vmm_as_activate(old_as);
    return 0;
}
