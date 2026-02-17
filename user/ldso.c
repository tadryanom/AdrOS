/* Userspace dynamic linker (ld.so) with lazy PLT/GOT binding.
 *
 * The kernel ELF loader pushes an auxiliary vector (auxv) onto the user
 * stack when PT_INTERP is present.  This linker:
 *   1. Parses auxv to find AT_PHDR, AT_PHNUM, AT_ENTRY
 *   2. Walks program headers to find PT_DYNAMIC
 *   3. Extracts DT_PLTGOT, DT_JMPREL, DT_PLTRELSZ, DT_SYMTAB, DT_STRTAB
 *   4. Sets GOT[1] = link_map pointer, GOT[2] = _dl_runtime_resolve
 *   5. Jumps to AT_ENTRY (the real program entry point)
 *
 * On first PLT call, the resolver fires: looks up the symbol, patches
 * the GOT entry, and jumps to the resolved function.  Subsequent calls
 * go directly through the patched GOT (zero overhead).
 *
 * The kernel loads DT_NEEDED shared libraries at SHLIB_BASE (0x20000000).
 * The resolver scans the .so's dynamic symtab to find undefined symbols. */

typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef int            int32_t;

/* ---- Auxiliary vector types ---- */
#define AT_NULL   0
#define AT_PHDR   3
#define AT_PHENT  4
#define AT_PHNUM  5
#define AT_ENTRY  9

/* ---- ELF types (minimal, matching kernel include/elf.h) ---- */
#define PT_LOAD    1
#define PT_DYNAMIC 2

#define DT_NULL    0
#define DT_NEEDED  1
#define DT_PLTRELSZ 2
#define DT_PLTGOT  3
#define DT_HASH    4
#define DT_STRTAB  5
#define DT_SYMTAB  6
#define DT_STRSZ   10
#define DT_SYMENT  11
#define DT_REL     17
#define DT_RELSZ   18
#define DT_JMPREL  23

#define R_386_32       1
#define R_386_COPY     5
#define R_386_GLOB_DAT 6
#define R_386_JMP_SLOT 7

#define ELF32_R_SYM(i)  ((i) >> 8)
#define ELF32_R_TYPE(i)  ((unsigned char)(i))

#define STB_GLOBAL 1
#define STB_WEAK   2
#define ELF32_ST_BIND(i) ((i) >> 4)

#define SHLIB_BASE 0x11000000U

struct elf32_phdr {
    uint32_t p_type, p_offset, p_vaddr, p_paddr;
    uint32_t p_filesz, p_memsz, p_flags, p_align;
};

struct elf32_dyn {
    int32_t  d_tag;
    uint32_t d_val;
};

struct elf32_rel {
    uint32_t r_offset;
    uint32_t r_info;
};

struct elf32_sym {
    uint32_t st_name, st_value, st_size;
    uint8_t  st_info, st_other;
    uint16_t st_shndx;
};

/* ---- Link map: per-module metadata for the resolver ---- */
struct link_map {
    uint32_t l_addr;           /* base load address (0 for ET_EXEC) */
    uint32_t jmprel;           /* DT_JMPREL VA (relocation table for .rel.plt) */
    uint32_t pltrelsz;         /* DT_PLTRELSZ */
    uint32_t symtab;           /* DT_SYMTAB VA */
    uint32_t strtab;           /* DT_STRTAB VA */
    uint32_t rel;              /* DT_REL VA (eager relocations) */
    uint32_t relsz;            /* DT_RELSZ */
    /* Shared lib symbol lookup info */
    uint32_t shlib_symtab;     /* .so DT_SYMTAB VA (0 if no .so) */
    uint32_t shlib_strtab;     /* .so DT_STRTAB VA */
    uint32_t shlib_base;       /* .so load base */
    uint32_t shlib_hash;       /* .so DT_HASH VA */
};

static struct link_map g_map;

/* ---- Minimal string helpers (no libc) ---- */
static int str_eq(const char* a, const char* b) {
    while (*a && *b) { if (*a++ != *b++) return 0; }
    return *a == *b;
}

/* ---- ELF hash (for DT_HASH lookup) ---- */
static uint32_t elf_hash(const char* name) {
    uint32_t h = 0, g;
    while (*name) {
        h = (h << 4) + (uint8_t)*name++;
        g = h & 0xF0000000U;
        if (g) h ^= g >> 24;
        h &= ~g;
    }
    return h;
}

/* ---- Symbol lookup in a shared library via DT_HASH ---- */
static uint32_t shlib_lookup(const char* name, const struct link_map* map) {
    if (!map->shlib_symtab || !map->shlib_strtab || !map->shlib_hash)
        return 0;

    const uint32_t* hashtab = (const uint32_t*)(map->shlib_hash + map->shlib_base);
    uint32_t nbucket = hashtab[0];
    uint32_t nchain  = hashtab[1];
    const uint32_t* bucket = &hashtab[2];
    const uint32_t* chain  = &hashtab[2 + nbucket];
    (void)nchain;

    uint32_t h = elf_hash(name) % nbucket;
    const struct elf32_sym* symtab = (const struct elf32_sym*)(map->shlib_symtab + map->shlib_base);
    const char* strtab = (const char*)(map->shlib_strtab + map->shlib_base);

    for (uint32_t i = bucket[h]; i != 0; i = chain[i]) {
        const struct elf32_sym* sym = &symtab[i];
        uint8_t bind = ELF32_ST_BIND(sym->st_info);
        if ((bind == STB_GLOBAL || bind == STB_WEAK) &&
            sym->st_shndx != 0 && sym->st_value != 0) {
            if (str_eq(strtab + sym->st_name, name))
                return sym->st_value + map->shlib_base;
        }
    }
    return 0;
}

/* ---- dl_fixup: called by _dl_runtime_resolve trampoline ----
 * Resolves a single PLT entry: looks up the symbol, patches GOT,
 * returns the resolved address. */
uint32_t dl_fixup(struct link_map* map, uint32_t reloc_offset)
    __attribute__((used, visibility("hidden")));

uint32_t dl_fixup(struct link_map* map, uint32_t reloc_offset) {
    const struct elf32_rel* rel =
        (const struct elf32_rel*)(map->jmprel + reloc_offset);

    uint32_t sym_idx = ELF32_R_SYM(rel->r_info);
    const struct elf32_sym* sym =
        &((const struct elf32_sym*)map->symtab)[sym_idx];

    uint32_t resolved = 0;

    if (sym->st_value != 0) {
        resolved = sym->st_value + map->l_addr;
    } else {
        const char* name = (const char*)map->strtab + sym->st_name;
        resolved = shlib_lookup(name, map);
    }

    if (resolved) {
        uint32_t* got_entry = (uint32_t*)(rel->r_offset + map->l_addr);
        *got_entry = resolved;
    }

    return resolved;
}

/* ---- _dl_runtime_resolve: PLT[0] jumps here via GOT[2] ----
 * Entry stack: [link_map*] [reloc_offset] [return_addr]
 * Uses the glibc i386 convention: save eax/ecx/edx, call dl_fixup,
 * restore, ret $8 to jump to resolved function. */
void _dl_runtime_resolve(void)
    __attribute__((naked, used, visibility("hidden")));

void _dl_runtime_resolve(void) {
    __asm__ volatile(
        "pushl %%eax\n"
        "pushl %%ecx\n"
        "pushl %%edx\n"
        "movl 16(%%esp), %%edx\n"   /* reloc_offset */
        "movl 12(%%esp), %%eax\n"   /* link_map* */
        "pushl %%edx\n"
        "pushl %%eax\n"
        "call dl_fixup\n"
        "addl $8, %%esp\n"
        "popl %%edx\n"
        "popl %%ecx\n"
        "xchgl %%eax, (%%esp)\n"    /* restore eax, put resolved addr on stack */
        "ret $8\n"                   /* jump to resolved; pop link_map + reloc_offset */
        ::: "memory"
    );
}

/* ---- Parse a PT_DYNAMIC at the given VA to extract .so symtab info ---- */
static void parse_shlib_dynamic(uint32_t dyn_va, uint32_t base) {
    const struct elf32_dyn* d = (const struct elf32_dyn*)dyn_va;
    for (; d->d_tag != DT_NULL; d++) {
        switch (d->d_tag) {
        case DT_SYMTAB: g_map.shlib_symtab = d->d_val; break;
        case DT_STRTAB: g_map.shlib_strtab = d->d_val; break;
        case DT_HASH:   g_map.shlib_hash   = d->d_val; break;
        }
    }
    g_map.shlib_base = base;
}

/* ---- Scan for shared library's PT_DYNAMIC at SHLIB_BASE ---- */
static void find_shlib_info(void) {
    const uint8_t* base = (const uint8_t*)SHLIB_BASE;
    /* Check ELF magic at SHLIB_BASE */
    if (base[0] != 0x7F || base[1] != 'E' || base[2] != 'L' || base[3] != 'F')
        return;

    uint32_t e_phoff   = *(const uint32_t*)(base + 28);
    uint16_t e_phnum   = *(const uint16_t*)(base + 44);
    uint16_t e_phentsize = *(const uint16_t*)(base + 42);

    for (uint16_t i = 0; i < e_phnum; i++) {
        const struct elf32_phdr* ph =
            (const struct elf32_phdr*)(base + e_phoff + i * e_phentsize);
        if (ph->p_type == PT_DYNAMIC) {
            parse_shlib_dynamic(ph->p_vaddr + SHLIB_BASE, SHLIB_BASE);
            return;
        }
    }
}

/* ---- Entry point ---- */
static void _start_c(uint32_t* initial_sp) __attribute__((noreturn, used));

void _start(void) __attribute__((noreturn, naked, section(".text.start")));
void _start(void) {
    __asm__ volatile(
        "pushl %%esp\n"
        "call _start_c\n"
        ::: "memory"
    );
    __builtin_unreachable();
}

static void _start_c(uint32_t* initial_sp) {
    /* Stack layout set by execve:
     *   initial_sp → argc
     *                argv[0], argv[1], ..., NULL
     *                envp[0], envp[1], ..., NULL
     *                auxv[0], auxv[1], ..., {AT_NULL, 0}  */
    uint32_t* sp = initial_sp;

    uint32_t argc = *sp++;
    sp += argc + 1;          /* skip argv[] + NULL terminator */
    while (*sp) sp++;         /* skip envp[] entries */
    sp++;                     /* skip envp NULL terminator */

    /* sp now points to auxv array */
    uint32_t at_entry = 0;
    uint32_t at_phdr  = 0;
    uint32_t at_phnum = 0;
    uint32_t at_phent = 0;

    for (uint32_t* p = sp; p[0] != AT_NULL; p += 2) {
        switch (p[0]) {
        case AT_ENTRY: at_entry = p[1]; break;
        case AT_PHDR:  at_phdr  = p[1]; break;
        case AT_PHNUM: at_phnum = p[1]; break;
        case AT_PHENT: at_phent = p[1]; break;
        }
    }

    if (!at_entry) {
        __asm__ volatile("mov $2, %%eax\n mov $127, %%ebx\n int $0x80" ::: "eax", "ebx");
        __builtin_unreachable();
    }

    /* Walk program headers to find PT_DYNAMIC */
    g_map.l_addr = 0;

    if (at_phdr && at_phnum && at_phent) {
        for (uint32_t i = 0; i < at_phnum; i++) {
            const struct elf32_phdr* ph =
                (const struct elf32_phdr*)(at_phdr + i * at_phent);
            if (ph->p_type == PT_DYNAMIC) {
                uint32_t dyn_va = ph->p_vaddr + g_map.l_addr;
                const struct elf32_dyn* d = (const struct elf32_dyn*)dyn_va;
                uint32_t pltgot = 0;

                for (; d->d_tag != DT_NULL; d++) {
                    switch (d->d_tag) {
                    case DT_PLTGOT:   pltgot          = d->d_val; break;
                    case DT_JMPREL:   g_map.jmprel    = d->d_val; break;
                    case DT_PLTRELSZ: g_map.pltrelsz  = d->d_val; break;
                    case DT_SYMTAB:   g_map.symtab    = d->d_val; break;
                    case DT_STRTAB:   g_map.strtab    = d->d_val; break;
                    case DT_REL:      g_map.rel       = d->d_val; break;
                    case DT_RELSZ:    g_map.relsz     = d->d_val; break;
                    }
                }

                /* Scan for shared library info BEFORE resolving relocations */
                find_shlib_info();

                /* Set up GOT for lazy binding:
                 * GOT[0] = _DYNAMIC (already set by linker)
                 * GOT[1] = link_map pointer
                 * GOT[2] = _dl_runtime_resolve address */
                if (pltgot && g_map.jmprel) {
                    uint32_t* got = (uint32_t*)(pltgot + g_map.l_addr);
                    got[1] = (uint32_t)&g_map;
                    got[2] = (uint32_t)&_dl_runtime_resolve;
                }

                /* Process eager relocations (R_386_GLOB_DAT, R_386_COPY) */
                if (g_map.rel && g_map.relsz) {
                    uint32_t nrel = g_map.relsz / sizeof(struct elf32_rel);
                    const struct elf32_rel* rtab =
                        (const struct elf32_rel*)(g_map.rel + g_map.l_addr);
                    for (uint32_t j = 0; j < nrel; j++) {
                        uint32_t type = ELF32_R_TYPE(rtab[j].r_info);
                        uint32_t sidx = ELF32_R_SYM(rtab[j].r_info);
                        uint32_t* target = (uint32_t*)(rtab[j].r_offset + g_map.l_addr);
                        if (type == R_386_GLOB_DAT || type == R_386_JMP_SLOT) {
                            const struct elf32_sym* s =
                                &((const struct elf32_sym*)g_map.symtab)[sidx];
                            uint32_t addr = 0;
                            if (s->st_value != 0)
                                addr = s->st_value + g_map.l_addr;
                            else {
                                const char* nm = (const char*)g_map.strtab + s->st_name;
                                addr = shlib_lookup(nm, &g_map);
                            }
                            if (addr) *target = addr;
                        } else if (type == R_386_COPY && sidx) {
                            const struct elf32_sym* s =
                                &((const struct elf32_sym*)g_map.symtab)[sidx];
                            const char* nm = (const char*)g_map.strtab + s->st_name;
                            uint32_t src = shlib_lookup(nm, &g_map);
                            if (src && s->st_size > 0) {
                                const uint8_t* sp = (const uint8_t*)src;
                                uint8_t* dp = (uint8_t*)target;
                                for (uint32_t k = 0; k < s->st_size; k++)
                                    dp[k] = sp[k];
                            }
                        }
                    }
                }
                break;
            }
        }
    }

    /* Restore the original stack pointer so the real program's _start
     * sees the correct layout: [argc] [argv...] [NULL] [envp...] [NULL] [auxv...]
     * Then jump to the program entry point. */
    __asm__ volatile(
        "mov %0, %%esp\n"
        "jmp *%1\n"
        :: "r"(initial_sp), "r"(at_entry)
        : "memory"
    );
    __builtin_unreachable();
}
