/* 
 * Defines the structures and prototypes needed to multitask.
 * Based on code from JamesM's kernel development tutorials.
 * Rewritten by Tulio Mendes to AdrOS Kernel
 */

#ifndef __TASK_H
#define __TASK_H 1

#include <typedefs.h>
#include <paging.h>

#define KERNEL_STACK_SIZE 2048       // Use a 2kb kernel stack.

// This structure defines a 'task' - a process.
typedef struct task
{
    s32int id;                // Process ID.
    u32int esp, ebp;       // Stack and base pointers.
    u32int eip;            // Instruction pointer.
    page_directory_t *page_directory; // Page directory.
    u32int kernel_stack;   // Kernel stack location.
    struct task *next;     // The next task in a linked list.
} task_t;

// Initialises the tasking system.
void initialise_tasking (void);

// Called by the timer hook, this changes the running process.
//void task_switch (void);
void switch_task (void);

/*
 * Forks the current process, spawning a new one with a different
 * memory space.
*/
int fork (void);

// Causes the current process' stack to be forcibly moved to a new location.
void move_stack (void *, u32int);

// Returns the pid of the current process.
int getpid (void);

#endif
