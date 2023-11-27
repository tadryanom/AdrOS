#ifndef __SYSTEM_H
#define __SYSTEM_H 1

#include <typedefs.h>

u8int inportb (u16int);
u16int inportw (u16int);
void outportb (u16int, u8int);
void outportw (u16int, u16int);

#endif
