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
 * AdrOS Host-Side Unit Tests for Security-Critical Functions
 *
 * Tests: user_range_ok, bitmap operations, eflags sanitization logic
 *
 * Compile: gcc -m32 -Iinclude -o build/test_security tests/test_security.c
 * Run:     ./build/test_security
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
    printf("  %-44s ", #name); \
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

#define ASSERT_TRUE(x) ASSERT_EQ(!!(x), 1)
#define ASSERT_FALSE(x) ASSERT_EQ(!!(x), 0)

/* ---- Functions under test ---- */

/* From src/kernel/uaccess.c (weak default, with the fix applied) */
#define USER_ADDR_LIMIT 0xC0000000U

static int user_range_ok(const void* user_ptr, size_t len) {
    uintptr_t uaddr = (uintptr_t)user_ptr;
    if (len == 0) return 1;
    if (uaddr == 0) return 0;
    uintptr_t end = uaddr + len - 1;
    if (end < uaddr) return 0;              /* overflow */
    if (uaddr >= USER_ADDR_LIMIT) return 0; /* kernel address */
    if (end >= USER_ADDR_LIMIT) return 0;   /* spans into kernel */
    return 1;
}

/* From src/mm/pmm.c — bitmap operations */
static uint8_t test_bitmap[16]; /* 128 bits = 128 frames */

static void bitmap_set(uint64_t bit) {
    test_bitmap[bit / 8] |= (uint8_t)(1 << (bit % 8));
}

static void bitmap_unset(uint64_t bit) {
    test_bitmap[bit / 8] &= (uint8_t)~(1 << (bit % 8));
}

static int bitmap_test(uint64_t bit) {
    return test_bitmap[bit / 8] & (1 << (bit % 8));
}

/* eflags sanitization logic from sigreturn fix */
static uint32_t sanitize_eflags(uint32_t eflags) {
    return (eflags & ~0x3000U) | 0x200U;
}

/* Signal mask logic from kernel/scheduler.c and kernel/syscall.c */
#define SIGKILL  9
#define SIGSTOP  19
#define PROCESS_MAX_SIG 32

static uint32_t sig_valid_mask(void) {
    /* SIGKILL and SIGSTOP cannot be blocked */
    uint32_t all = 0;
    for (int i = 1; i < PROCESS_MAX_SIG; i++) {
        if (i != SIGKILL && i != SIGSTOP)
            all |= (1U << (uint32_t)i);
    }
    return all;
}

static uint32_t sig_pending_and_blocked(uint32_t pending, uint32_t blocked) {
    /* sigpending returns: pending & blocked */
    return pending & blocked;
}

static int sig_is_deliverable(uint32_t sig, uint32_t blocked) {
    /* Signal is deliverable if not blocked (except KILL/STOP always deliverable) */
    if (sig == (uint32_t)SIGKILL || sig == (uint32_t)SIGSTOP) return 1;
    return (blocked & (1U << sig)) == 0;
}

/* chmod symbolic mode parsing from user/cmds/chmod/chmod.c */
static unsigned int parse_symbolic(const char* mode, unsigned int old) {
    unsigned int result = old;
    const char* p = mode;
    while (*p) {
        unsigned int who_bits = 0;
        int has_u = 0, has_g = 0, has_o = 0;
        while (*p == 'u' || *p == 'g' || *p == 'o' || *p == 'a') {
            switch (*p) {
            case 'u': has_u = 1; break;
            case 'g': has_g = 1; break;
            case 'o': has_o = 1; break;
            case 'a': has_u = has_g = has_o = 1; break;
            }
            p++;
        }
        if (!has_u && !has_g && !has_o) has_u = has_g = has_o = 1;

        if (has_u) who_bits |= 04700;
        if (has_g) who_bits |= 02070;
        if (has_o) who_bits |= 00007;

        while (*p) {
            char op = *p;
            if (op != '+' && op != '-' && op != '=') break;
            p++;

            unsigned int perm = 0;
            while (*p == 'r' || *p == 'w' || *p == 'x' ||
                   *p == 's' || *p == 't') {
                switch (*p) {
                case 'r':
                    if (has_u) perm |= 0400;
                    if (has_g) perm |= 0040;
                    if (has_o) perm |= 0004;
                    break;
                case 'w':
                    if (has_u) perm |= 0200;
                    if (has_g) perm |= 0020;
                    if (has_o) perm |= 0002;
                    break;
                case 'x':
                    if (has_u) perm |= 0100;
                    if (has_g) perm |= 0010;
                    if (has_o) perm |= 0001;
                    break;
                case 's':
                    if (has_u) perm |= 04000;
                    if (has_g) perm |= 02000;
                    break;
                case 't':
                    perm |= 01000;
                    break;
                }
                p++;
            }

            if (op == '+') {
                result |= perm;
            } else if (op == '-') {
                result &= ~perm;
            } else {
                result &= ~who_bits;
                result |= perm;
            }

            if (*p == ',') { p++; break; }
        }
    }
    return result & 07777;
}

/* ======== user_range_ok TESTS ======== */

TEST(urange_null_ptr) {
    ASSERT_FALSE(user_range_ok(NULL, 10));
}

TEST(urange_zero_len) {
    ASSERT_TRUE(user_range_ok(NULL, 0));
    ASSERT_TRUE(user_range_ok((void*)0x1000, 0));
}

TEST(urange_valid_user) {
    ASSERT_TRUE(user_range_ok((void*)0x08048000, 4096));
}

TEST(urange_kernel_addr) {
    ASSERT_FALSE(user_range_ok((void*)0xC0000000, 1));
}

TEST(urange_kernel_addr_high) {
    ASSERT_FALSE(user_range_ok((void*)0xC0100000, 100));
}

TEST(urange_spans_boundary) {
    /* Start in user space, end in kernel space */
    ASSERT_FALSE(user_range_ok((void*)0xBFFFF000, 0x2000));
}

TEST(urange_just_below_limit) {
    ASSERT_TRUE(user_range_ok((void*)0xBFFFFFFF, 1));
}

TEST(urange_at_limit) {
    ASSERT_FALSE(user_range_ok((void*)0xC0000000, 1));
}

TEST(urange_overflow) {
    /* uaddr + len wraps around */
    ASSERT_FALSE(user_range_ok((void*)0xFFFFFFFF, 2));
}

TEST(urange_max_user) {
    /* Largest valid user range: 0x1 to 0xBFFFFFFF */
    ASSERT_TRUE(user_range_ok((void*)0x1, 0xBFFFFFFF));
}

TEST(urange_max_user_plus_one) {
    /* 0x1 + 0xC0000000 - 1 = 0xC0000000 which is kernel */
    ASSERT_FALSE(user_range_ok((void*)0x1, 0xC0000000));
}

/* ======== Bitmap TESTS ======== */

TEST(bitmap_set_and_test) {
    memset(test_bitmap, 0, sizeof(test_bitmap));
    bitmap_set(0);
    ASSERT_TRUE(bitmap_test(0));
    ASSERT_FALSE(bitmap_test(1));
}

TEST(bitmap_unset) {
    memset(test_bitmap, 0xFF, sizeof(test_bitmap));
    bitmap_unset(7);
    ASSERT_FALSE(bitmap_test(7));
    ASSERT_TRUE(bitmap_test(6));
    ASSERT_TRUE(bitmap_test(8));
}

TEST(bitmap_cross_byte) {
    memset(test_bitmap, 0, sizeof(test_bitmap));
    bitmap_set(7);  /* last bit of byte 0 */
    bitmap_set(8);  /* first bit of byte 1 */
    ASSERT_TRUE(bitmap_test(7));
    ASSERT_TRUE(bitmap_test(8));
    ASSERT_FALSE(bitmap_test(6));
    ASSERT_FALSE(bitmap_test(9));
}

TEST(bitmap_all_bits) {
    memset(test_bitmap, 0, sizeof(test_bitmap));
    for (int i = 0; i < 128; i++) {
        bitmap_set((uint64_t)i);
    }
    for (int i = 0; i < 128; i++) {
        ASSERT_TRUE(bitmap_test((uint64_t)i));
    }
    /* Unset every other bit */
    for (int i = 0; i < 128; i += 2) {
        bitmap_unset((uint64_t)i);
    }
    for (int i = 0; i < 128; i++) {
        if (i % 2 == 0) {
            ASSERT_FALSE(bitmap_test((uint64_t)i));
        } else {
            ASSERT_TRUE(bitmap_test((uint64_t)i));
        }
    }
}

/* ======== eflags sanitization TESTS ======== */

TEST(eflags_clears_iopl) {
    /* IOPL=3 (bits 12-13 set) should be cleared */
    uint32_t dirty = 0x3000 | 0x200; /* IOPL=3, IF=1 */
    uint32_t clean = sanitize_eflags(dirty);
    ASSERT_EQ(clean & 0x3000, 0); /* IOPL cleared */
    ASSERT_TRUE(clean & 0x200);   /* IF still set */
}

TEST(eflags_sets_if) {
    /* IF=0 should be forced to IF=1 */
    uint32_t dirty = 0; /* no flags */
    uint32_t clean = sanitize_eflags(dirty);
    ASSERT_TRUE(clean & 0x200); /* IF set */
}

TEST(eflags_preserves_other) {
    /* CF=1 (bit 0), ZF=1 (bit 6), SF=1 (bit 7) should be preserved */
    uint32_t dirty = 0x3000 | 0x01 | 0x40 | 0x80; /* IOPL=3, CF, ZF, SF */
    uint32_t clean = sanitize_eflags(dirty);
    ASSERT_TRUE(clean & 0x01);  /* CF preserved */
    ASSERT_TRUE(clean & 0x40);  /* ZF preserved */
    ASSERT_TRUE(clean & 0x80);  /* SF preserved */
    ASSERT_EQ(clean & 0x3000, 0); /* IOPL cleared */
    ASSERT_TRUE(clean & 0x200);   /* IF set */
}

TEST(eflags_iopl1) {
    /* IOPL=1 should also be cleared */
    uint32_t dirty = 0x1000 | 0x200;
    uint32_t clean = sanitize_eflags(dirty);
    ASSERT_EQ(clean & 0x3000, 0);
}

/* ======== Signal mask TESTS ======== */

TEST(sigmask_valid_excludes_kill) {
    /* SIGKILL (bit 9) must not be in valid mask */
    uint32_t mask = sig_valid_mask();
    ASSERT_EQ(mask & (1U << SIGKILL), 0);
}

TEST(sigmask_valid_excludes_stop) {
    /* SIGSTOP (bit 19) must not be in valid mask */
    uint32_t mask = sig_valid_mask();
    ASSERT_EQ(mask & (1U << SIGSTOP), 0);
}

TEST(sigmask_valid_includes_usr1) {
    /* SIGUSR1 (bit 10) must be in valid mask */
    uint32_t mask = sig_valid_mask();
    ASSERT_TRUE(mask & (1U << 10));
}

TEST(sigmask_pending_and_blocked) {
    /* sigpending = pending & blocked */
    uint32_t pending = (1U << 10) | (1U << 12); /* SIGUSR1, SIGUSR2 */
    uint32_t blocked = (1U << 10);              /* only SIGUSR1 blocked */
    uint32_t result = sig_pending_and_blocked(pending, blocked);
    ASSERT_EQ(result, (1U << 10));               /* only SIGUSR1 pending+blocked */
}

TEST(sigmask_pending_not_blocked) {
    /* Signal pending but not blocked → not in sigpending result */
    uint32_t pending = (1U << 10);
    uint32_t blocked = 0;
    uint32_t result = sig_pending_and_blocked(pending, blocked);
    ASSERT_EQ(result, 0);
}

TEST(sigmask_deliverable_normal) {
    /* Normal signal not blocked → deliverable */
    ASSERT_TRUE(sig_is_deliverable(10, 0));
}

TEST(sigmask_deliverable_blocked) {
    /* Normal signal blocked → not deliverable */
    ASSERT_FALSE(sig_is_deliverable(10, (1U << 10)));
}

TEST(sigmask_kill_always_deliverable) {
    /* SIGKILL always deliverable even if blocked */
    ASSERT_TRUE(sig_is_deliverable(SIGKILL, (1U << SIGKILL)));
}

TEST(sigmask_stop_always_deliverable) {
    /* SIGSTOP always deliverable even if blocked */
    ASSERT_TRUE(sig_is_deliverable(SIGSTOP, (1U << SIGSTOP)));
}

/* ======== chmod symbolic mode TESTS ======== */

TEST(chmod_u_plus_x) {
    /* u+x on 0644 → 0744 (only user gets execute) */
    ASSERT_EQ(parse_symbolic("u+x", 0644), 0744);
}

TEST(chmod_go_minus_w) {
    /* go-w on 0666 → 0644 */
    ASSERT_EQ(parse_symbolic("go-w", 0666), 0644);
}

TEST(chmod_a_plus_x) {
    /* a+x on 0644 → 0755 */
    ASSERT_EQ(parse_symbolic("a+x", 0644), 0755);
}

TEST(chmod_a_eq_rw) {
    /* a=rw on 0755 → 0666 */
    ASSERT_EQ(parse_symbolic("a=rw", 0755), 0666);
}

TEST(chmod_u_plus_s) {
    /* u+s on 0755 → 4755 (setuid) */
    ASSERT_EQ(parse_symbolic("u+s", 0755), 04755);
}

TEST(chmod_g_plus_s) {
    /* g+s on 0755 → 2755 (setgid) */
    ASSERT_EQ(parse_symbolic("g+s", 0755), 02755);
}

TEST(chmod_plus_t) {
    /* +t on 0777 → 1777 (sticky) */
    ASSERT_EQ(parse_symbolic("+t", 0777), 01777);
}

TEST(chmod_o_minus_x) {
    /* o-x on 0755 → 0754 */
    ASSERT_EQ(parse_symbolic("o-x", 0755), 0754);
}

TEST(chmod_u_eq_rw_go_eq_r) {
    /* u=rw,go=r on 0755 → 0644 */
    ASSERT_EQ(parse_symbolic("u=rw,go=r", 0755), 0644);
}

TEST(chmod_no_change) {
    /* u+r on already 0755 → 0755 */
    ASSERT_EQ(parse_symbolic("u+r", 0755), 0755);
}

/* ======== MAIN ======== */
int main(void) {
    printf("\n=========================================\n");
    printf("  AdrOS Security Unit Tests\n");
    printf("=========================================\n\n");

    /* user_range_ok */
    RUN(urange_null_ptr);
    RUN(urange_zero_len);
    RUN(urange_valid_user);
    RUN(urange_kernel_addr);
    RUN(urange_kernel_addr_high);
    RUN(urange_spans_boundary);
    RUN(urange_just_below_limit);
    RUN(urange_at_limit);
    RUN(urange_overflow);
    RUN(urange_max_user);
    RUN(urange_max_user_plus_one);

    /* bitmap */
    RUN(bitmap_set_and_test);
    RUN(bitmap_unset);
    RUN(bitmap_cross_byte);
    RUN(bitmap_all_bits);

    /* eflags */
    RUN(eflags_clears_iopl);
    RUN(eflags_sets_if);
    RUN(eflags_preserves_other);
    RUN(eflags_iopl1);

    /* signal mask */
    RUN(sigmask_valid_excludes_kill);
    RUN(sigmask_valid_excludes_stop);
    RUN(sigmask_valid_includes_usr1);
    RUN(sigmask_pending_and_blocked);
    RUN(sigmask_pending_not_blocked);
    RUN(sigmask_deliverable_normal);
    RUN(sigmask_deliverable_blocked);
    RUN(sigmask_kill_always_deliverable);
    RUN(sigmask_stop_always_deliverable);

    /* chmod symbolic mode */
    RUN(chmod_u_plus_x);
    RUN(chmod_go_minus_w);
    RUN(chmod_a_plus_x);
    RUN(chmod_a_eq_rw);
    RUN(chmod_u_plus_s);
    RUN(chmod_g_plus_s);
    RUN(chmod_plus_t);
    RUN(chmod_o_minus_x);
    RUN(chmod_u_eq_rw_go_eq_r);
    RUN(chmod_no_change);

    printf("\n  %d/%d passed, %d failed\n", g_tests_passed, g_tests_run, g_tests_failed);

    if (g_tests_failed > 0) {
        printf("  RESULT: FAIL\n\n");
        return 1;
    }
    printf("  RESULT: PASS\n\n");
    return 0;
}
