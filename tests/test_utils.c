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
 * AdrOS Host-Side Unit Tests for Pure Functions
 *
 * Compile: gcc -m32 -Iinclude -o build/test_utils tests/test_utils.c
 * Run:     ./build/test_utils
 *
 * Tests kernel utility functions that have no hardware dependency.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ---- Minimal test framework ---- */
static int g_tests_run = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST(name) static void test_##name(void)
#define RUN(name) do { \
    g_tests_run++; \
    printf("  %-40s ", #name); \
    test_##name(); \
    printf("PASS\n"); \
    g_tests_passed++; \
} while(0)

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        printf("FAIL\n    %s:%d: %d != %d\n", __FILE__, __LINE__, (int)(a), (int)(b)); \
        g_tests_failed++; \
        return; \
    } \
} while(0)

#define ASSERT_STR_EQ(a, b) do { \
    if (strcmp((a), (b)) != 0) { \
        printf("FAIL\n    %s:%d: \"%s\" != \"%s\"\n", __FILE__, __LINE__, (a), (b)); \
        g_tests_failed++; \
        return; \
    } \
} while(0)

/* ---- Functions under test (copied from kernel sources) ---- */

/* From src/kernel/utils.c */
static void reverse(char* str, int length) {
    int start = 0;
    int end = length - 1;
    while (start < end) {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        start++;
        end--;
    }
}

static void itoa(int num, char* str, int base) {
    int i = 0;
    int isNegative = 0;

    if (num == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return;
    }

    if (num < 0 && base == 10) {
        isNegative = 1;
        num = -num;
    }

    while (num != 0) {
        int rem = num % base;
        str[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
        num = num / base;
    }

    if (isNegative)
        str[i++] = '-';

    str[i] = '\0';
    reverse(str, i);
}

static int atoi_k(const char* str) {
    int res = 0;
    int sign = 1;
    int i = 0;

    if (str[0] == '-') {
        sign = -1;
        i++;
    }

    for (; str[i] != '\0'; ++i) {
        if (str[i] >= '0' && str[i] <= '9')
            res = res * 10 + str[i] - '0';
    }

    return sign * res;
}

static void itoa_hex(uint32_t num, char* str) {
    const char hex_chars[] = "0123456789ABCDEF";
    str[0] = '0';
    str[1] = 'x';

    for (int i = 0; i < 8; i++) {
        str[9 - i] = hex_chars[num & 0xF];
        num >>= 4;
    }
    str[10] = '\0';
}

/* From src/kernel/syscall.c — path_normalize_inplace */
static void path_normalize_inplace(char* s) {
    if (!s) return;
    if (s[0] == 0) {
        strcpy(s, "/");
        return;
    }

    char tmp[128];
    size_t comp_start[32];
    int depth = 0;
    size_t w = 0;

    const char* p = s;
    int absolute = (*p == '/');
    if (absolute) {
        tmp[w++] = '/';
        while (*p == '/') p++;
    }

    while (*p != 0) {
        const char* seg = p;
        while (*p != 0 && *p != '/') p++;
        size_t seg_len = (size_t)(p - seg);
        while (*p == '/') p++;

        if (seg_len == 1 && seg[0] == '.') {
            continue;
        }

        if (seg_len == 2 && seg[0] == '.' && seg[1] == '.') {
            if (depth > 0) {
                depth--;
                w = comp_start[depth];
            }
            continue;
        }

        if (depth < 32) {
            comp_start[depth++] = w;
        }

        if (w > 1 || (w == 1 && tmp[0] != '/')) {
            if (w + 1 < sizeof(tmp)) tmp[w++] = '/';
        }

        for (size_t i = 0; i < seg_len && w + 1 < sizeof(tmp); i++) {
            tmp[w++] = seg[i];
        }
    }

    if (w == 0) {
        tmp[w++] = '/';
    }

    while (w > 1 && tmp[w - 1] == '/') {
        w--;
    }

    tmp[w] = 0;
    strcpy(s, tmp);
}

/* From src/mm/pmm.c — align helpers */
static uint64_t align_down(uint64_t value, uint64_t align) {
    return value & ~(align - 1);
}

static uint64_t align_up(uint64_t value, uint64_t align) {
    return (value + align - 1) & ~(align - 1);
}

/* From src/drivers/initrd.c — tar octal parsing */
static uint32_t tar_parse_octal(const char* s, size_t n) {
    uint32_t v = 0;
    for (size_t i = 0; i < n; i++) {
        if (s[i] == 0) break;
        if (s[i] < '0' || s[i] > '7') continue;
        v = (v << 3) + (uint32_t)(s[i] - '0');
    }
    return v;
}

/* From src/kernel/fs.c — mount point prefix check */
static int path_is_mountpoint_prefix(const char* mp, const char* path) {
    size_t mpl = strlen(mp);
    if (mpl == 0) return 0;
    if (strcmp(mp, "/") == 0) return 1;
    if (strncmp(path, mp, mpl) != 0) return 0;
    if (path[mpl] == 0) return 1;
    if (path[mpl] == '/') return 1;
    return 0;
}

/* From src/kernel/fs.c — mountpoint normalization */
static void normalize_mountpoint(const char* in, char* out, size_t out_sz) {
    if (!out || out_sz == 0) return;
    out[0] = 0;
    if (!in || in[0] == 0) {
        strcpy(out, "/");
        return;
    }
    size_t i = 0;
    if (in[0] != '/') {
        out[i++] = '/';
    }
    for (size_t j = 0; in[j] != 0 && i + 1 < out_sz; j++) {
        out[i++] = in[j];
    }
    out[i] = 0;
    size_t l = strlen(out);
    while (l > 1 && out[l - 1] == '/') {
        out[l - 1] = 0;
        l--;
    }
}

/* From src/kernel/syscall.c — permission check (simplified for host test) */
#define EACCES 13
static int vfs_check_permission(uint32_t mode, uint32_t euid, uint32_t egid,
                                 uint32_t file_uid, uint32_t file_gid, int want) {
    if (euid == 0) return 0;       /* root — allow all */
    if (mode == 0) return 0;        /* mode not set — permissive */
    uint32_t perm;
    if (euid == file_uid) {
        perm = (mode >> 6) & 7;
    } else if (egid == file_gid) {
        perm = (mode >> 3) & 7;
    } else {
        perm = mode & 7;
    }
    if (((uint32_t)want & perm) != (uint32_t)want) return -EACCES;
    return 0;
}

/* From src/arch/x86/elf.c — ELF header validation (simplified for host) */
#define ELF_MAGIC0 0x7F
#define ELF_MAGIC1 'E'
#define ELF_MAGIC2 'L'
#define ELF_MAGIC3 'F'
#define ELFCLASS32 1
#define ELFDATA2LSB 1
#define ET_EXEC 2
#define ET_DYN 3
#define EM_386 3
#define EINVAL 22
#define EFAULT 14

typedef struct {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint32_t e_entry;
    uint32_t e_phoff;
    uint32_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} test_elf32_ehdr_t;

typedef struct {
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
} test_elf32_phdr_t;

static int elf32_validate(const test_elf32_ehdr_t* eh, size_t file_len) {
    if (!eh) return -EFAULT;
    if (file_len < sizeof(*eh)) return -EINVAL;
    if (eh->e_ident[0] != ELF_MAGIC0 ||
        eh->e_ident[1] != ELF_MAGIC1 ||
        eh->e_ident[2] != ELF_MAGIC2 ||
        eh->e_ident[3] != ELF_MAGIC3)
        return -EINVAL;
    if (eh->e_ident[4] != ELFCLASS32) return -EINVAL;
    if (eh->e_ident[5] != ELFDATA2LSB) return -EINVAL;
    if (eh->e_type != ET_EXEC && eh->e_type != ET_DYN) return -EINVAL;
    if (eh->e_machine != EM_386) return -EINVAL;
    if (eh->e_phentsize != sizeof(test_elf32_phdr_t)) return -EINVAL;
    if (eh->e_phnum == 0) return -EINVAL;
    return 0;
}

/* ======== TESTS ======== */

/* --- itoa tests --- */
TEST(itoa_zero) {
    char buf[16];
    itoa(0, buf, 10);
    ASSERT_STR_EQ(buf, "0");
}

TEST(itoa_positive) {
    char buf[16];
    itoa(12345, buf, 10);
    ASSERT_STR_EQ(buf, "12345");
}

TEST(itoa_negative) {
    char buf[16];
    itoa(-42, buf, 10);
    ASSERT_STR_EQ(buf, "-42");
}

TEST(itoa_hex_base) {
    char buf[16];
    itoa(255, buf, 16);
    ASSERT_STR_EQ(buf, "ff");
}

TEST(itoa_one) {
    char buf[16];
    itoa(1, buf, 10);
    ASSERT_STR_EQ(buf, "1");
}

TEST(itoa_large) {
    char buf[16];
    itoa(2147483647, buf, 10);
    ASSERT_STR_EQ(buf, "2147483647");
}

/* --- itoa_hex tests --- */
TEST(itoa_hex_zero) {
    char buf[16];
    itoa_hex(0, buf);
    ASSERT_STR_EQ(buf, "0x00000000");
}

TEST(itoa_hex_deadbeef) {
    char buf[16];
    itoa_hex(0xDEADBEEF, buf);
    ASSERT_STR_EQ(buf, "0xDEADBEEF");
}

TEST(itoa_hex_small) {
    char buf[16];
    itoa_hex(0xFF, buf);
    ASSERT_STR_EQ(buf, "0x000000FF");
}

/* --- atoi tests --- */
TEST(atoi_zero) {
    ASSERT_EQ(atoi_k("0"), 0);
}

TEST(atoi_positive) {
    ASSERT_EQ(atoi_k("12345"), 12345);
}

TEST(atoi_negative) {
    ASSERT_EQ(atoi_k("-99"), -99);
}

TEST(atoi_leading_garbage) {
    /* atoi_k skips non-digit chars after optional sign */
    ASSERT_EQ(atoi_k("abc"), 0);
}

/* --- path_normalize_inplace tests --- */
TEST(path_root) {
    char p[128] = "/";
    path_normalize_inplace(p);
    ASSERT_STR_EQ(p, "/");
}

TEST(path_empty) {
    char p[128] = "";
    path_normalize_inplace(p);
    ASSERT_STR_EQ(p, "/");
}

TEST(path_simple) {
    char p[128] = "/foo/bar";
    path_normalize_inplace(p);
    ASSERT_STR_EQ(p, "/foo/bar");
}

TEST(path_trailing_slash) {
    char p[128] = "/foo/bar/";
    path_normalize_inplace(p);
    ASSERT_STR_EQ(p, "/foo/bar");
}

TEST(path_double_slash) {
    char p[128] = "/foo//bar";
    path_normalize_inplace(p);
    ASSERT_STR_EQ(p, "/foo/bar");
}

TEST(path_dot) {
    char p[128] = "/foo/./bar";
    path_normalize_inplace(p);
    ASSERT_STR_EQ(p, "/foo/bar");
}

TEST(path_dotdot) {
    char p[128] = "/foo/bar/../baz";
    path_normalize_inplace(p);
    ASSERT_STR_EQ(p, "/foo/baz");
}

TEST(path_dotdot_root) {
    char p[128] = "/foo/..";
    path_normalize_inplace(p);
    ASSERT_STR_EQ(p, "/");
}

TEST(path_dotdot_beyond_root) {
    char p[128] = "/../..";
    path_normalize_inplace(p);
    ASSERT_STR_EQ(p, "/");
}

TEST(path_complex) {
    char p[128] = "/a/b/c/../../d/./e/../f";
    path_normalize_inplace(p);
    ASSERT_STR_EQ(p, "/a/d/f");
}

/* --- align tests --- */
TEST(align_down_basic) {
    ASSERT_EQ(align_down(4097, 4096), 4096);
}

TEST(align_down_exact) {
    ASSERT_EQ(align_down(4096, 4096), 4096);
}

TEST(align_up_basic) {
    ASSERT_EQ(align_up(4097, 4096), 8192);
}

TEST(align_up_exact) {
    ASSERT_EQ(align_up(4096, 4096), 4096);
}

TEST(align_up_zero) {
    ASSERT_EQ(align_up(0, 4096), 0);
}

/* --- tar_parse_octal tests --- */
TEST(tar_octal_zero) {
    ASSERT_EQ(tar_parse_octal("0", 2), 0);
}

TEST(tar_octal_simple) {
    ASSERT_EQ(tar_parse_octal("644", 4), 0644);
}

TEST(tar_octal_large) {
    ASSERT_EQ(tar_parse_octal("100644", 7), 0100644);
}

TEST(tar_octal_empty) {
    ASSERT_EQ(tar_parse_octal("", 1), 0);
}

TEST(tar_octal_null_term) {
    /* Stops at NUL even if n is larger */
    ASSERT_EQ(tar_parse_octal("755\0ignored", 10), 0755);
}

TEST(tar_octal_skips_non_octal) {
    /* Space padding is common in tar headers */
    ASSERT_EQ(tar_parse_octal(" 644 ", 6), 0644);
}

/* --- path_is_mountpoint_prefix tests --- */
TEST(mountprefix_root) {
    ASSERT_EQ(path_is_mountpoint_prefix("/", "/anything"), 1);
}

TEST(mountprefix_exact) {
    ASSERT_EQ(path_is_mountpoint_prefix("/disk", "/disk"), 1);
}

TEST(mountprefix_child) {
    ASSERT_EQ(path_is_mountpoint_prefix("/disk", "/disk/file"), 1);
}

TEST(mountprefix_no_match) {
    ASSERT_EQ(path_is_mountpoint_prefix("/disk", "/other"), 0);
}

TEST(mountprefix_partial) {
    /* /dis should not match /disk */
    ASSERT_EQ(path_is_mountpoint_prefix("/dis", "/disk"), 0);
}

TEST(mountprefix_empty) {
    ASSERT_EQ(path_is_mountpoint_prefix("", "/anything"), 0);
}

/* --- normalize_mountpoint tests --- */
TEST(normmount_root) {
    char out[128];
    normalize_mountpoint("/", out, sizeof(out));
    ASSERT_STR_EQ(out, "/");
}

TEST(normmount_simple) {
    char out[128];
    normalize_mountpoint("disk", out, sizeof(out));
    ASSERT_STR_EQ(out, "/disk");
}

TEST(normmount_trailing_slash) {
    char out[128];
    normalize_mountpoint("/disk/", out, sizeof(out));
    ASSERT_STR_EQ(out, "/disk");
}

TEST(normmount_empty) {
    char out[128];
    normalize_mountpoint("", out, sizeof(out));
    ASSERT_STR_EQ(out, "/");
}

TEST(normmount_null) {
    char out[128];
    normalize_mountpoint(NULL, out, sizeof(out));
    ASSERT_STR_EQ(out, "/");
}

TEST(normmount_already_normalized) {
    char out[128];
    normalize_mountpoint("/disk", out, sizeof(out));
    ASSERT_STR_EQ(out, "/disk");
}

/* --- vfs_check_permission tests --- */
TEST(perm_root_allows) {
    /* Root can do anything */
    ASSERT_EQ(vfs_check_permission(0100, 0, 0, 1000, 1000, 4), 0);
}

TEST(perm_owner_read) {
    /* mode=0400, owner wants read → allowed */
    ASSERT_EQ(vfs_check_permission(0400, 1000, 100, 1000, 100, 4), 0);
}

TEST(perm_owner_no_write) {
    /* mode=0400, owner wants write → denied */
    ASSERT_EQ(vfs_check_permission(0400, 1000, 100, 1000, 100, 2), -EACCES);
}

TEST(perm_group_read) {
    /* mode=0040, group member wants read → allowed */
    ASSERT_EQ(vfs_check_permission(0040, 2000, 100, 1000, 100, 4), 0);
}

TEST(perm_other_read) {
    /* mode=0004, other wants read → allowed */
    ASSERT_EQ(vfs_check_permission(0004, 3000, 200, 1000, 100, 4), 0);
}

TEST(perm_other_no_write) {
    /* mode=0004, other wants write → denied */
    ASSERT_EQ(vfs_check_permission(0004, 3000, 200, 1000, 100, 2), -EACCES);
}

TEST(perm_mode_zero_permissive) {
    /* mode=0 → permissive (allow all) */
    ASSERT_EQ(vfs_check_permission(0, 1000, 100, 1000, 100, 7), 0);
}

TEST(perm_rw_file) {
    /* mode=0644: owner rw, group/other read */
    ASSERT_EQ(vfs_check_permission(0644, 1000, 100, 1000, 100, 6), 0); /* owner rw */
    ASSERT_EQ(vfs_check_permission(0644, 2000, 100, 1000, 100, 4), 0); /* group read */
    ASSERT_EQ(vfs_check_permission(0644, 2000, 100, 1000, 100, 2), -EACCES); /* group no write */
    ASSERT_EQ(vfs_check_permission(0644, 3000, 200, 1000, 100, 4), 0); /* other read */
    ASSERT_EQ(vfs_check_permission(0644, 3000, 200, 1000, 100, 2), -EACCES); /* other no write */
}

/* --- elf32_validate tests --- */
TEST(elf_valid_exec) {
    test_elf32_ehdr_t eh;
    memset(&eh, 0, sizeof(eh));
    eh.e_ident[0] = 0x7F;
    eh.e_ident[1] = 'E';
    eh.e_ident[2] = 'L';
    eh.e_ident[3] = 'F';
    eh.e_ident[4] = ELFCLASS32;
    eh.e_ident[5] = ELFDATA2LSB;
    eh.e_type = ET_EXEC;
    eh.e_machine = EM_386;
    eh.e_phentsize = sizeof(test_elf32_phdr_t);
    eh.e_phnum = 1;
    ASSERT_EQ(elf32_validate(&eh, sizeof(eh)), 0);
}

TEST(elf_valid_dyn) {
    test_elf32_ehdr_t eh;
    memset(&eh, 0, sizeof(eh));
    eh.e_ident[0] = 0x7F;
    eh.e_ident[1] = 'E';
    eh.e_ident[2] = 'L';
    eh.e_ident[3] = 'F';
    eh.e_ident[4] = ELFCLASS32;
    eh.e_ident[5] = ELFDATA2LSB;
    eh.e_type = ET_DYN;
    eh.e_machine = EM_386;
    eh.e_phentsize = sizeof(test_elf32_phdr_t);
    eh.e_phnum = 1;
    ASSERT_EQ(elf32_validate(&eh, sizeof(eh)), 0);
}

TEST(elf_bad_magic) {
    test_elf32_ehdr_t eh;
    memset(&eh, 0, sizeof(eh));
    eh.e_ident[0] = 'X';
    ASSERT_EQ(elf32_validate(&eh, sizeof(eh)), -EINVAL);
}

TEST(elf_bad_class) {
    test_elf32_ehdr_t eh;
    memset(&eh, 0, sizeof(eh));
    eh.e_ident[0] = 0x7F;
    eh.e_ident[1] = 'E';
    eh.e_ident[2] = 'L';
    eh.e_ident[3] = 'F';
    eh.e_ident[4] = 2; /* ELFCLASS64 */
    eh.e_ident[5] = ELFDATA2LSB;
    eh.e_type = ET_EXEC;
    eh.e_machine = EM_386;
    eh.e_phentsize = sizeof(test_elf32_phdr_t);
    eh.e_phnum = 1;
    ASSERT_EQ(elf32_validate(&eh, sizeof(eh)), -EINVAL);
}

TEST(elf_bad_type) {
    test_elf32_ehdr_t eh;
    memset(&eh, 0, sizeof(eh));
    eh.e_ident[0] = 0x7F;
    eh.e_ident[1] = 'E';
    eh.e_ident[2] = 'L';
    eh.e_ident[3] = 'F';
    eh.e_ident[4] = ELFCLASS32;
    eh.e_ident[5] = ELFDATA2LSB;
    eh.e_type = 0; /* ET_NONE */
    eh.e_machine = EM_386;
    eh.e_phentsize = sizeof(test_elf32_phdr_t);
    eh.e_phnum = 1;
    ASSERT_EQ(elf32_validate(&eh, sizeof(eh)), -EINVAL);
}

TEST(elf_bad_machine) {
    test_elf32_ehdr_t eh;
    memset(&eh, 0, sizeof(eh));
    eh.e_ident[0] = 0x7F;
    eh.e_ident[1] = 'E';
    eh.e_ident[2] = 'L';
    eh.e_ident[3] = 'F';
    eh.e_ident[4] = ELFCLASS32;
    eh.e_ident[5] = ELFDATA2LSB;
    eh.e_type = ET_EXEC;
    eh.e_machine = 0x3E; /* EM_X86_64 */
    eh.e_phentsize = sizeof(test_elf32_phdr_t);
    eh.e_phnum = 1;
    ASSERT_EQ(elf32_validate(&eh, sizeof(eh)), -EINVAL);
}

TEST(elf_no_phnum) {
    test_elf32_ehdr_t eh;
    memset(&eh, 0, sizeof(eh));
    eh.e_ident[0] = 0x7F;
    eh.e_ident[1] = 'E';
    eh.e_ident[2] = 'L';
    eh.e_ident[3] = 'F';
    eh.e_ident[4] = ELFCLASS32;
    eh.e_ident[5] = ELFDATA2LSB;
    eh.e_type = ET_EXEC;
    eh.e_machine = EM_386;
    eh.e_phentsize = sizeof(test_elf32_phdr_t);
    eh.e_phnum = 0;
    ASSERT_EQ(elf32_validate(&eh, sizeof(eh)), -EINVAL);
}

TEST(elf_truncated) {
    test_elf32_ehdr_t eh;
    memset(&eh, 0, sizeof(eh));
    ASSERT_EQ(elf32_validate(&eh, 10), -EINVAL);
}

TEST(elf_null) {
    ASSERT_EQ(elf32_validate(NULL, 100), -EFAULT);
}

/* ======== MAIN ======== */
int main(void) {
    printf("\n=========================================\n");
    printf("  AdrOS Host-Side Unit Tests\n");
    printf("=========================================\n\n");

    /* itoa */
    RUN(itoa_zero);
    RUN(itoa_positive);
    RUN(itoa_negative);
    RUN(itoa_hex_base);
    RUN(itoa_one);
    RUN(itoa_large);

    /* itoa_hex */
    RUN(itoa_hex_zero);
    RUN(itoa_hex_deadbeef);
    RUN(itoa_hex_small);

    /* atoi */
    RUN(atoi_zero);
    RUN(atoi_positive);
    RUN(atoi_negative);
    RUN(atoi_leading_garbage);

    /* path_normalize */
    RUN(path_root);
    RUN(path_empty);
    RUN(path_simple);
    RUN(path_trailing_slash);
    RUN(path_double_slash);
    RUN(path_dot);
    RUN(path_dotdot);
    RUN(path_dotdot_root);
    RUN(path_dotdot_beyond_root);
    RUN(path_complex);

    /* align */
    RUN(align_down_basic);
    RUN(align_down_exact);
    RUN(align_up_basic);
    RUN(align_up_exact);
    RUN(align_up_zero);

    /* tar_parse_octal */
    RUN(tar_octal_zero);
    RUN(tar_octal_simple);
    RUN(tar_octal_large);
    RUN(tar_octal_empty);
    RUN(tar_octal_null_term);
    RUN(tar_octal_skips_non_octal);

    /* path_is_mountpoint_prefix */
    RUN(mountprefix_root);
    RUN(mountprefix_exact);
    RUN(mountprefix_child);
    RUN(mountprefix_no_match);
    RUN(mountprefix_partial);
    RUN(mountprefix_empty);

    /* normalize_mountpoint */
    RUN(normmount_root);
    RUN(normmount_simple);
    RUN(normmount_trailing_slash);
    RUN(normmount_empty);
    RUN(normmount_null);
    RUN(normmount_already_normalized);

    /* vfs_check_permission */
    RUN(perm_root_allows);
    RUN(perm_owner_read);
    RUN(perm_owner_no_write);
    RUN(perm_group_read);
    RUN(perm_other_read);
    RUN(perm_other_no_write);
    RUN(perm_mode_zero_permissive);
    RUN(perm_rw_file);

    /* elf32_validate */
    RUN(elf_valid_exec);
    RUN(elf_valid_dyn);
    RUN(elf_bad_magic);
    RUN(elf_bad_class);
    RUN(elf_bad_type);
    RUN(elf_bad_machine);
    RUN(elf_no_phnum);
    RUN(elf_truncated);
    RUN(elf_null);

    printf("\n  %d/%d passed, %d failed\n", g_tests_passed, g_tests_run, g_tests_failed);

    if (g_tests_failed > 0) {
        printf("  RESULT: FAIL\n\n");
        return 1;
    }
    printf("  RESULT: PASS\n\n");
    return 0;
}
