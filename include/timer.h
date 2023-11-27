/* 
 * Defines the interface for all PIT-related functions.
 * Based on code from JamesM's kernel development tutorials.
 * Rewritten by Tulio Mendes to AdrOS Kernel
 */

#ifndef __TIMER_H
#define __TIMER_H 1

#include <typedefs.h>

void init_timer (u32int);
void init_rtc (void);
void sleep_ms (u32int);

#endif
