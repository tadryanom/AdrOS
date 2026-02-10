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

    printf("\n  %d/%d passed, %d failed\n", g_tests_passed, g_tests_run, g_tests_failed);

    if (g_tests_failed > 0) {
        printf("  RESULT: FAIL\n\n");
        return 1;
    }
    printf("  RESULT: PASS\n\n");
    return 0;
}
