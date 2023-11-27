/* 
 * Defines the interface for and structures relating to paging.
 * Based on code from JamesM's kernel development tutorials.
 * Rewritten by Tulio Mendes to AdrOS Kernel
 */

#ifndef __PAGING_H
#define __PAGING_H 1

#include <typedefs.h>
#include <isr.h>

#define TOTAL_PAGES 1024

typedef struct page
{
    u32int present    : 1;   // Page present in memory
    u32int rw         : 1;   // Read-only if clear, readwrite if set
    u32int user       : 1;   // Supervisor level only if clear
    u32int accessed   : 1;   // Has the page been accessed since last refresh?
    u32int dirty      : 1;   // Has the page been written to since last refresh?
    u32int unused     : 7;   // Amalgamation of unused and reserved bits
    u32int frame      : 20;  // Frame address (shifted right 12 bits)
} page_t;

typedef struct page_table
{
    page_t pages[TOTAL_PAGES];
} page_table_t;

typedef struct page_directory
{
    page_table_t *tables[TOTAL_PAGES]; // Array of pointers to pagetables.
    /*
     * Array of pointers to the pagetables above, but gives their *physical*
     * location, for loading into the CR3 register.
     */
    u32int physicalTables[TOTAL_PAGES];

    /*
     * The physical address of tablesPhysical. This comes into play
     * when we get our kernel heap allocated and the directory
     * may be in a different location in virtual memory.
     */
    u32int physicalAddr;
} page_directory_t;

/*
 * Sets up the environment, page directories etc and
 * enables paging.
 */
void initialise_paging (void);

/*
 * Causes the specified page directory to be loaded into the
 * CR3 register.
 */
void switch_page_directory (page_directory_t *);

/*
 * Retrieves a pointer to the page required.
 * If make == 1, if the page-table in which this page should
 * reside isn't created, create it!
 */
page_t *get_page (u32int, s32int, page_directory_t *);

// Handler for page faults.
void page_fault (registers_t *);

// Makes a copy of a page directory.
page_directory_t *clone_directory (page_directory_t *);

// Function to allocate a frame.
void alloc_frame (page_t *, s32int, s32int);

// Function to deallocate a frame.
void free_frame (page_t *);

extern void copy_page_physical (u32int, u32int); 
#endif
