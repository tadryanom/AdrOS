/* 
 * Defines the interface for and structures relating to the initial ramdisk.
 * Based on code from JamesM's kernel development tutorials.
 * Rewritten by Tulio Mendes to AdrOS Kernel
 */

#ifndef __INITRD_H
#define __INITRD_H 1

#include <typedefs.h>

#include <typedefs.h>
#include <fs.h>

typedef struct
{
    u32int nfiles; // The number of files in the ramdisk.
} initrd_header_t;

typedef struct
{
    u8int magic;     // Magic number, for error checking.
    s8int name[64];  // Filename.
    u32int offset;   // Offset in the initrd that the file starts.
    u32int length;   // Length of the file.
} initrd_file_header_t;

/*
 * Initialises the initial ramdisk. It gets passed the address of the multiboot module,
 * and returns a completed filesystem node.
 */
fs_node_t *initialise_initrd (u32int location);

#endif
