#include "string.h"

void* memcpy(void* dst, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;
    for (size_t i = 0; i < n; i++) d[i] = s[i];
    return dst;
}

void* memset(void* s, int c, size_t n) {
    uint8_t* p = (uint8_t*)s;
    for (size_t i = 0; i < n; i++) p[i] = (uint8_t)c;
    return s;
}

void* memmove(void* dst, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;
    if (d < s) {
        for (size_t i = 0; i < n; i++) d[i] = s[i];
    } else {
        for (size_t i = n; i > 0; i--) d[i-1] = s[i-1];
    }
    return dst;
}

int memcmp(const void* a, const void* b, size_t n) {
    const uint8_t* x = (const uint8_t*)a;
    const uint8_t* y = (const uint8_t*)b;
    for (size_t i = 0; i < n; i++) {
        if (x[i] != y[i]) return (int)x[i] - (int)y[i];
    }
    return 0;
}

size_t strlen(const char* s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

char* strcpy(char* dst, const char* src) {
    size_t i = 0;
    while (src[i]) { dst[i] = src[i]; i++; }
    dst[i] = 0;
    return dst;
}

char* strncpy(char* dst, const char* src, size_t n) {
    size_t i = 0;
    while (i < n && src[i]) { dst[i] = src[i]; i++; }
    while (i < n) { dst[i] = 0; i++; }
    return dst;
}

int strcmp(const char* a, const char* b) {
    while (*a && *a == *b) { a++; b++; }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

int strncmp(const char* a, const char* b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i]) return (int)(unsigned char)a[i] - (int)(unsigned char)b[i];
        if (a[i] == 0) break;
    }
    return 0;
}

char* strchr(const char* s, int c) {
    while (*s) {
        if (*s == (char)c) return (char*)s;
        s++;
    }
    return (c == 0) ? (char*)s : (void*)0;
}

char* strrchr(const char* s, int c) {
    const char* last = (void*)0;
    while (*s) {
        if (*s == (char)c) last = s;
        s++;
    }
    if (c == 0) return (char*)s;
    return (char*)last;
}

char* strcat(char* dst, const char* src) {
    char* p = dst;
    while (*p) p++;
    while (*src) *p++ = *src++;
    *p = 0;
    return dst;
}

char* strncat(char* dst, const char* src, size_t n) {
    char* p = dst;
    while (*p) p++;
    size_t i = 0;
    while (i < n && src[i]) { *p++ = src[i++]; }
    *p = 0;
    return dst;
}

char* strdup(const char* s) {
    if (!s) return (void*)0;
    size_t len = strlen(s) + 1;
    extern void* malloc(size_t);
    char* d = (char*)malloc(len);
    if (d) memcpy(d, s, len);
    return d;
}

static int tolower_impl(int c) {
    return (c >= 'A' && c <= 'Z') ? c + 32 : c;
}

int strcasecmp(const char* a, const char* b) {
    while (*a && tolower_impl((unsigned char)*a) == tolower_impl((unsigned char)*b)) { a++; b++; }
    return tolower_impl((unsigned char)*a) - tolower_impl((unsigned char)*b);
}

int strncasecmp(const char* a, const char* b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        int ca = tolower_impl((unsigned char)a[i]);
        int cb = tolower_impl((unsigned char)b[i]);
        if (ca != cb) return ca - cb;
        if (a[i] == 0) break;
    }
    return 0;
}

char* strstr(const char* haystack, const char* needle) {
    if (!*needle) return (char*)haystack;
    size_t nlen = strlen(needle);
    while (*haystack) {
        if (strncmp(haystack, needle, nlen) == 0) return (char*)haystack;
        haystack++;
    }
    return (void*)0;
}

void* memchr(const void* s, int c, size_t n) {
    const uint8_t* p = (const uint8_t*)s;
    for (size_t i = 0; i < n; i++) {
        if (p[i] == (uint8_t)c) return (void*)(p + i);
    }
    return (void*)0;
}

static char* strtok_state = (void*)0;

char* strtok(char* str, const char* delim) {
    if (str) strtok_state = str;
    if (!strtok_state) return (void*)0;
    /* skip leading delimiters */
    while (*strtok_state && strchr(delim, *strtok_state)) strtok_state++;
    if (!*strtok_state) return (void*)0;
    char* start = strtok_state;
    while (*strtok_state && !strchr(delim, *strtok_state)) strtok_state++;
    if (*strtok_state) *strtok_state++ = 0;
    return start;
}
