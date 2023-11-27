#ifndef __SYSTEM_H
#define __SYSTEM_H 1

#include <typedefs.h>

#define PANIC(msg) panic(msg, __FILE__, __LINE__);
#define ASSERT(b) ((b) ? (void)0 : panic_assert(__FILE__, __LINE__, #b))

u8int inportb (u16int);
u16int inportw (u16int);
void outportb (u16int, u8int);
void outportw (u16int, u16int);

void panic (const s8int *, const s8int *, u32int);
void panic_assert (const s8int *, u32int, const s8int *);

#endif
