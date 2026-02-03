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

char* strcpy(char* dest, const char* src) {
    char* temp = dest;
    while ((*dest++ = *src++));
    return temp;
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
