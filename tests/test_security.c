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

    printf("\n  %d/%d passed, %d failed\n", g_tests_passed, g_tests_run, g_tests_failed);

    if (g_tests_failed > 0) {
        printf("  RESULT: FAIL\n\n");
        return 1;
    }
    printf("  RESULT: PASS\n\n");
    return 0;
}
