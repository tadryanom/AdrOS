#ifndef __STRING_H
#define __STRING_H 1

#include <typedefs.h>

extern u8int *memcpy (u8int *, const u8int *, s32int);
extern u8int *memset (u8int *, u8int, s32int);
extern u16int *memsetw (u16int *, u16int, s32int);
extern s32int strlen (const s8int *);

#endif
