#ifndef __KMAIN_H
#define __KMAIN_H 1

#include <typedefs.h>
#include <multiboot.h>
#include <screen.h>
#include <stdio.h>
#include <descriptors.h>
#include <timer.h>

void kmain (u64int, u64int, u32int);

#endif
