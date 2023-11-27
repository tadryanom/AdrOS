/* 
 * Defines the interface for all PIT-related functions.
 * Based on code from Bran's and JamesM's kernel development tutorials.
 * Rewritten by Tulio Mendes to AdrOS Kernel
 */

#ifndef __TIMER_H
#define __TIMER_H 1

#include <typedefs.h>

void init_timer (u32int);
void sleep_ms (u32int);

#endif
