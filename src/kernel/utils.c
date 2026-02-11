#include "utils.h"

size_t strlen(const char* str) {
    size_t len = 0;
    while (str[len])
        len++;
    return len;
}

int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

int strncmp(const char* s1, const char* s2, size_t n) {
    while (n && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) return 0;
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

void* memset(void* ptr, int value, size_t num) {
    unsigned char* p = ptr;
    while (num--)
        *p++ = (unsigned char)value;
    return ptr;
}

void* memcpy(void* dst, const void* src, size_t n) {
    char* d = dst;
    const char* s = src;
    while (n--)
        *d++ = *s++;
    return dst;
}

void* memmove(void* dst, const void* src, size_t n) {
    unsigned char* d = dst;
    const unsigned char* s = src;
    if (d < s) {
        while (n--) *d++ = *s++;
    } else {
        d += n; s += n;
        while (n--) *--d = *--s;
    }
    return dst;
}

int memcmp(const void* a, const void* b, size_t n) {
    const unsigned char* pa = a;
    const unsigned char* pb = b;
    for (size_t i = 0; i < n; i++) {
        if (pa[i] != pb[i]) return (int)pa[i] - (int)pb[i];
    }
    return 0;
}

char* strcpy(char* dest, const char* src) {
    char* saved = dest;
    while (*src) {
        *dest++ = *src++;
    }
    *dest = 0;
    return saved;
}

void reverse(char* str, int length) {
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

void itoa(int num, char* str, int base) {
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

int atoi(const char* str) {
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

char* strncpy(char* dest, const char* src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i]; i++) dest[i] = src[i];
    for (; i < n; i++) dest[i] = '\0';
    return dest;
}

long strtol(const char* nptr, char** endptr, int base) {
    long result = 0;
    int neg = 0;
    const char* p = nptr;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '-') { neg = 1; p++; }
    else if (*p == '+') p++;
    if (base == 0) {
        if (*p == '0' && (p[1] == 'x' || p[1] == 'X')) { base = 16; p += 2; }
        else if (*p == '0') { base = 8; p++; }
        else base = 10;
    } else if (base == 16 && *p == '0' && (p[1] == 'x' || p[1] == 'X')) {
        p += 2;
    }
    while (*p) {
        int digit;
        if (*p >= '0' && *p <= '9') digit = *p - '0';
        else if (*p >= 'a' && *p <= 'f') digit = *p - 'a' + 10;
        else if (*p >= 'A' && *p <= 'F') digit = *p - 'A' + 10;
        else break;
        if (digit >= base) break;
        result = result * base + digit;
        p++;
    }
    if (endptr) *endptr = (char*)p;
    return neg ? -result : result;
}

/* GCC fortified memcpy — use builtin to avoid infinite recursion */
void* __memcpy_chk(void* dst, const void* src, size_t n, size_t dst_len) {
    (void)dst_len;
    char* d = dst;
    const char* s = src;
    while (n--) *d++ = *s++;
    return dst;
}

/* GCC ctype locale stub — lwIP ip4_addr_c uses isdigit/isxdigit */
static const unsigned short _ctype_table[384] = {0};
static const unsigned short* _ctype_ptr = &_ctype_table[128];
const unsigned short** __ctype_b_loc(void) {
    return (const unsigned short**)&_ctype_ptr;
}

void itoa_hex(uint32_t num, char* str) {
    const char hex_chars[] = "0123456789ABCDEF";
    str[0] = '0';
    str[1] = 'x';
    
    for (int i = 0; i < 8; i++) {
        str[9 - i] = hex_chars[num & 0xF];
        num >>= 4;
    }
    str[10] = '\0';
}
