#include "stdlib.h"
#include "unistd.h"
#include "string.h"

/* Global environment pointer — set by crt0 from execve stack layout */
char** __environ = 0;

/*
 * Minimal bump allocator using brk() syscall.
 * No free() support yet — memory is only reclaimed on process exit.
 * A proper free-list allocator can be added later.
 */
static void* heap_base = 0;
static void* heap_end = 0;

void* malloc(size_t size) {
    if (size == 0) return (void*)0;

    /* Align to 8 bytes */
    size = (size + 7) & ~(size_t)7;

    if (!heap_base) {
        heap_base = brk(0);
        heap_end = heap_base;
    }

    void* old_end = heap_end;
    void* new_end = (void*)((char*)heap_end + size);
    void* result = brk(new_end);

    if ((uintptr_t)result < (uintptr_t)new_end) {
        return (void*)0;  /* OOM */
    }

    heap_end = new_end;
    return old_end;
}

void free(void* ptr) {
    /* Bump allocator: no-op for now */
    (void)ptr;
}

void* calloc(size_t nmemb, size_t size) {
    size_t total = nmemb * size;
    if (nmemb != 0 && total / nmemb != size) return (void*)0;
    void* p = malloc(total);
    if (p) memset(p, 0, total);
    return p;
}

void* realloc(void* ptr, size_t size) {
    if (!ptr) return malloc(size);
    if (size == 0) { free(ptr); return (void*)0; }
    /* Bump allocator: just allocate new and copy.
     * We don't know the old size, so copy 'size' bytes
     * (caller must ensure old block >= size). */
    void* new_ptr = malloc(size);
    if (new_ptr) memcpy(new_ptr, ptr, size);
    return new_ptr;
}

int atoi(const char* s) {
    int n = 0;
    int neg = 0;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '-') { neg = 1; s++; }
    else if (*s == '+') { s++; }
    while (*s >= '0' && *s <= '9') {
        n = n * 10 + (*s - '0');
        s++;
    }
    return neg ? -n : n;
}

char* realpath(const char* path, char* resolved) {
    if (!path || !resolved) return (void*)0;

    char tmp[256];
    int tpos = 0;

    if (path[0] != '/') {
        if (getcwd(tmp, sizeof(tmp)) < 0) return (void*)0;
        tpos = (int)strlen(tmp);
        if (tpos > 0 && tmp[tpos - 1] != '/') tmp[tpos++] = '/';
    }

    while (*path) {
        if (*path == '/') { path++; continue; }
        if (path[0] == '.' && (path[1] == '/' || path[1] == '\0')) {
            path += 1; continue;
        }
        if (path[0] == '.' && path[1] == '.' && (path[2] == '/' || path[2] == '\0')) {
            path += 2;
            if (tpos > 1) {
                tpos--;
                while (tpos > 0 && tmp[tpos - 1] != '/') tpos--;
            }
            continue;
        }
        if (tpos > 0 && tmp[tpos - 1] != '/') tmp[tpos++] = '/';
        while (*path && *path != '/' && tpos < 254) tmp[tpos++] = *path++;
    }

    if (tpos == 0) tmp[tpos++] = '/';
    tmp[tpos] = '\0';

    memcpy(resolved, tmp, (size_t)(tpos + 1));
    return resolved;
}

double atof(const char* s) {
    /* Minimal: parse integer part only (no FP hardware in AdrOS) */
    int neg = 0;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '-') { neg = 1; s++; }
    else if (*s == '+') { s++; }
    double val = 0.0;
    while (*s >= '0' && *s <= '9') { val = val * 10.0 + (*s - '0'); s++; }
    if (*s == '.') {
        s++;
        double frac = 0.1;
        while (*s >= '0' && *s <= '9') { val += (*s - '0') * frac; frac *= 0.1; s++; }
    }
    return neg ? -val : val;
}

long strtol(const char* nptr, char** endptr, int base) {
    const char* s = nptr;
    long result = 0;
    int neg = 0;

    while (*s == ' ' || *s == '\t') s++;
    if (*s == '-') { neg = 1; s++; }
    else if (*s == '+') { s++; }

    if (base == 0) {
        if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) { base = 16; s += 2; }
        else if (s[0] == '0') { base = 8; s++; }
        else { base = 10; }
    } else if (base == 16 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s += 2;
    }

    while (*s) {
        int digit;
        if (*s >= '0' && *s <= '9') digit = *s - '0';
        else if (*s >= 'a' && *s <= 'z') digit = *s - 'a' + 10;
        else if (*s >= 'A' && *s <= 'Z') digit = *s - 'A' + 10;
        else break;
        if (digit >= base) break;
        result = result * base + digit;
        s++;
    }

    if (endptr) *endptr = (char*)s;
    return neg ? -result : result;
}

char* getenv(const char* name) {
    extern char** __environ;
    if (!name || !__environ) return (char*)0;
    size_t len = strlen(name);
    for (char** e = __environ; *e; e++) {
        if (strncmp(*e, name, len) == 0 && (*e)[len] == '=')
            return *e + len + 1;
    }
    return (char*)0;
}

int abs(int x) {
    return x < 0 ? -x : x;
}

long labs(long x) {
    return x < 0 ? -x : x;
}

void qsort(void* base, size_t nmemb, size_t size,
           int (*compar)(const void*, const void*)) {
    if (nmemb < 2 || !base || !compar) return;
    char* b = (char*)base;
    char tmp[256];
    for (size_t i = 1; i < nmemb; i++) {
        size_t j = i;
        while (j > 0 && compar(b + (j - 1) * size, b + j * size) > 0) {
            /* swap elements — use stack buffer for small, byte-swap for large */
            char* a1 = b + (j - 1) * size;
            char* a2 = b + j * size;
            if (size <= sizeof(tmp)) {
                memcpy(tmp, a1, size);
                memcpy(a1, a2, size);
                memcpy(a2, tmp, size);
            } else {
                for (size_t k = 0; k < size; k++) {
                    char t = a1[k]; a1[k] = a2[k]; a2[k] = t;
                }
            }
            j--;
        }
    }
}

int system(const char* cmd) {
    (void)cmd;
    return -1;
}

void exit(int status) {
    _exit(status);
}
