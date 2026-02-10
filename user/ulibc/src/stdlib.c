#include "stdlib.h"
#include "unistd.h"
#include "string.h"

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

    if ((unsigned int)result < (unsigned int)new_end) {
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

void exit(int status) {
    _exit(status);
}
