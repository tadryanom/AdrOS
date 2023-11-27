#ifndef __STRING_H
#define __STRING_H 1

#include <typedefs.h>

u8int *memcpy (u8int *, const u8int *, s32int);
u8int *memset (u8int *, u8int, s32int);
u16int *memsetw (u16int *, u16int, s32int);
s32int strlen (const s8int *);
s32int strcmp (s8int *, s8int *);
s8int *strcpy (s8int *, const s8int *);
s8int *strcat (s8int *, const s8int *);


#endif
