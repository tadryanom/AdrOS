#ifndef __SYSTEM_H
#define __SYSTEM_H 1

#include <common.h>

extern void outportb (u16int, u8int);
extern u8int inportb (u16int);
extern u16int inportw (u16int);

#endif
