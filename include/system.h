#ifndef __SYSTEM_H
#define __SYSTEM_H 1

#include <typedefs.h>

extern u8int inportb (u16int);
extern u16int inportw (u16int);
extern void outportb (u16int, u8int);
extern void outportw (u16int, u16int);

#endif
