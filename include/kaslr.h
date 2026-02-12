#ifndef KASLR_H
#define KASLR_H

#include <stdint.h>

void kaslr_init(void);
uint32_t kaslr_rand(void);

/* Returns a page-aligned random offset in [0, max_pages * PAGE_SIZE). */
uint32_t kaslr_offset(uint32_t max_pages);

#endif
