// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#include "stdlib.h"
#include "unistd.h"
#include "string.h"

/* Global environment pointer — set by crt0 from execve stack layout.
 * POSIX name is 'environ'. */
char** environ = 0;

/*
 * Free-list allocator using brk() syscall.
 *
 * Each allocated block has a header: [size | in_use_flag]
 * Free blocks are linked in an explicit free list.
 * Coalescing is done on free() (merge with adjacent free blocks).
 *
 * Layout: [header 8 bytes] [user data ...]
 *   header.size = total block size including header (aligned to 8)
 *   header.next = pointer to next free block (only valid when free)
 */

#define ALLOC_ALIGN 8
#define ALLOC_HDR_SIZE 8  /* must == sizeof(struct block_hdr) */
#define ALLOC_USED_BIT 1U

struct block_hdr {
    uint32_t size;       /* total size including header; bit 0 = used flag */
    uint32_t next_free;  /* pointer to next free block (as uintptr_t), 0 = end */
};

static struct block_hdr* free_list = 0;
static void* heap_base = 0;
static void* heap_end = 0;

static inline uint32_t blk_size(struct block_hdr* b) { return b->size & ~ALLOC_USED_BIT; }
static inline int      blk_used(struct block_hdr* b) { return (int)(b->size & ALLOC_USED_BIT); }

static void* sbrk_grow(size_t inc) {
    if (!heap_base) {
        heap_base = brk(0);
        heap_end = heap_base;
    }
    void* old_end = heap_end;
    void* new_end = (void*)((char*)heap_end + inc);
    void* result = brk(new_end);
    if ((uintptr_t)result < (uintptr_t)new_end)
        return (void*)0;
    heap_end = new_end;
    return old_end;
}

void* malloc(size_t size) {
    if (size == 0) return (void*)0;

    /* Align to 8 bytes, add header */
    size = (size + ALLOC_ALIGN - 1) & ~(size_t)(ALLOC_ALIGN - 1);
    uint32_t total = (uint32_t)size + ALLOC_HDR_SIZE;

    /* First-fit search in free list */
    struct block_hdr** prev = &free_list;
    struct block_hdr* cur = free_list;
    while (cur) {
        uint32_t bsz = blk_size(cur);
        if (bsz >= total) {
            /* Split if remainder is large enough for another block */
            if (bsz >= total + ALLOC_HDR_SIZE + ALLOC_ALIGN) {
                struct block_hdr* split = (struct block_hdr*)((char*)cur + total);
                split->size = bsz - total;
                split->next_free = cur->next_free;
                *prev = split;
                cur->size = total | ALLOC_USED_BIT;
            } else {
                /* Use entire block */
                *prev = (struct block_hdr*)(uintptr_t)cur->next_free;
                cur->size = bsz | ALLOC_USED_BIT;
            }
            return (void*)((char*)cur + ALLOC_HDR_SIZE);
        }
        prev = (struct block_hdr**)&cur->next_free;
        cur = (struct block_hdr*)(uintptr_t)cur->next_free;
    }

    /* No free block found — grow heap */
    void* p = sbrk_grow(total);
    if (!p) return (void*)0;
    struct block_hdr* b = (struct block_hdr*)p;
    b->size = total | ALLOC_USED_BIT;
    b->next_free = 0;
    return (void*)((char*)b + ALLOC_HDR_SIZE);
}

void free(void* ptr) {
    if (!ptr) return;
    struct block_hdr* b = (struct block_hdr*)((char*)ptr - ALLOC_HDR_SIZE);
    b->size &= ~ALLOC_USED_BIT;  /* mark free */

    /* Insert into free list (address-ordered for coalescing) */
    struct block_hdr** prev = &free_list;
    struct block_hdr* cur = free_list;
    while (cur && (uintptr_t)cur < (uintptr_t)b) {
        prev = (struct block_hdr**)&cur->next_free;
        cur = (struct block_hdr*)(uintptr_t)cur->next_free;
    }

    b->next_free = (uint32_t)(uintptr_t)cur;
    *prev = b;

    /* Coalesce with next block if adjacent */
    if (cur && (char*)b + blk_size(b) == (char*)cur) {
        b->size += blk_size(cur);
        b->next_free = cur->next_free;
    }

    /* Coalesce with previous block if adjacent */
    struct block_hdr* p_prev = free_list;
    if (p_prev != b) {
        while (p_prev && (struct block_hdr*)(uintptr_t)p_prev->next_free != b)
            p_prev = (struct block_hdr*)(uintptr_t)p_prev->next_free;
        if (p_prev && (char*)p_prev + blk_size(p_prev) == (char*)b) {
            p_prev->size += blk_size(b);
            p_prev->next_free = b->next_free;
        }
    }
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

    struct block_hdr* b = (struct block_hdr*)((char*)ptr - ALLOC_HDR_SIZE);
    uint32_t old_usable = blk_size(b) - ALLOC_HDR_SIZE;

    if (old_usable >= size) return ptr;  /* already large enough */

    void* new_ptr = malloc(size);
    if (new_ptr) {
        memcpy(new_ptr, ptr, old_usable);
        free(ptr);
    }
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
        if (!getcwd(tmp, sizeof(tmp))) return (void*)0;
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
    extern char** environ;
    if (!name || !environ) return (char*)0;
    size_t len = strlen(name);
    for (char** e = environ; *e; e++) {
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

void* bsearch(const void* key, const void* base, size_t nmemb, size_t size,
              int (*compar)(const void*, const void*)) {
    const char* b = (const char*)base;
    size_t lo = 0, hi = nmemb;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        int cmp = compar(key, b + mid * size);
        if (cmp == 0) return (void*)(b + mid * size);
        if (cmp < 0) hi = mid;
        else lo = mid + 1;
    }
    return (void*)0;
}

div_t div(int numer, int denom) {
    div_t r;
    r.quot = numer / denom;
    r.rem  = numer % denom;
    return r;
}

ldiv_t ldiv(long numer, long denom) {
    ldiv_t r;
    r.quot = numer / denom;
    r.rem  = numer % denom;
    return r;
}

int mkstemp(char* tmpl) {
    size_t len = strlen(tmpl);
    if (len < 6) return -1;
    char* suffix = tmpl + len - 6;
    /* Check template ends with XXXXXX */
    for (int i = 0; i < 6; i++)
        if (suffix[i] != 'X') return -1;
    /* Generate unique name using pid + counter */
    extern int getpid(void);
    static int counter = 0;
    int id = getpid() * 1000 + (counter++ % 1000);
    for (int i = 5; i >= 0; i--) {
        suffix[i] = (char)('0' + id % 10);
        id /= 10;
    }
    int fd = open(tmpl, 1 | 0x40 | 0x80 /* O_WRONLY|O_CREAT|O_EXCL */);
    return fd;
}

double strtod(const char* nptr, char** endptr) {
    const char* s = nptr;
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
    if (*s == 'e' || *s == 'E') {
        s++;
        int eneg = 0;
        if (*s == '-') { eneg = 1; s++; }
        else if (*s == '+') { s++; }
        int exp = 0;
        while (*s >= '0' && *s <= '9') { exp = exp * 10 + (*s - '0'); s++; }
        double mult = 1.0;
        for (int i = 0; i < exp; i++) mult *= 10.0;
        if (eneg) val /= mult; else val *= mult;
    }
    if (endptr) *endptr = (char*)s;
    return neg ? -val : val;
}

float strtof(const char* nptr, char** endptr) {
    return (float)strtod(nptr, endptr);
}
