#ifndef HEAP_H
#define HEAP_H

#include <stddef.h>
#include <stdint.h>

void kheap_init(void);
void* kmalloc(size_t size);
void kfree(void* ptr);

#endif
