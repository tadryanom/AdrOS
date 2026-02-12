#ifndef ULIBC_STDLIB_H
#define ULIBC_STDLIB_H

#include <stddef.h>

void*   malloc(size_t size);
void    free(void* ptr);
void*   calloc(size_t nmemb, size_t size);
void*   realloc(void* ptr, size_t size);

int     atoi(const char* s);
char*   realpath(const char* path, char* resolved);
void    exit(int status) __attribute__((noreturn));

#ifndef NULL
#define NULL ((void*)0)
#endif

#endif
