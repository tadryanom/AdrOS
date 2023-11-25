#ifndef __SCREEN_H
#define __SCREEN_H 1

#include <common.h>

extern void scroll (void);
extern void move_cursor (void);
extern void init_video (void);
extern void cls (void);
extern void put_char (s8int);

#endif
