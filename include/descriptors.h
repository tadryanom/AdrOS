/* 
 * Defines the interface for initialising the GDT and IDT.
 * Also defines needed structures.
 * Based on code from Bran's and JamesM's kernel development tutorials.
 * Rewritten by Tulio Mendes to AdrOS Kernel
 */

#ifndef __DESCRIPTORS_H
#define __DESCRIPTORS_H 1

#include <typedefs.h>

// Initialisation function is publicly accessible.
void init_descriptors (void);

// Allows the kernel stack in the TSS to be changed.
void set_kernel_stack (u32int);

/*
 * This structure contains the value of one GDT entry.
 * We use the attribute 'packed' to tell GCC not to change
 * any of the alignment in the structure.
 */
struct gdt_entry
{
    u16int limit_low;           // The lower 16 bits of the limit.
    u16int base_low;            // The lower 16 bits of the base.
    u8int  base_middle;         // The next 8 bits of the base.
    u8int  access;              // Access flags, determine what ring this segment can be used in.
    u8int  granularity;
    u8int  base_high;           // The last 8 bits of the base.
} __attribute__((packed));

typedef struct gdt_entry gdt_entry_t;

/* 
 * This struct describes a GDT pointer. It points to the start of
 * our array of GDT entries, and is in the format required by the
 * lgdt instruction.
 */
struct gdt_ptr
{
    u16int limit;               // The upper 16 bits of all selector limits.
    u32int base;                // The address of the first gdt_entry_t struct.
} __attribute__((packed));

typedef struct gdt_ptr gdt_ptr_t;

// A struct describing a Task State Segment.
struct tss_entry
{
    u32int prev_tss;   // The previous TSS - if we used hardware task switching this would form a linked list.
    u32int esp0;       // The stack pointer to load when we change to kernel mode.
    u32int ss0;        // The stack segment to load when we change to kernel mode.
    u32int esp1;       // Unused...
    u32int ss1;
    u32int esp2;  
    u32int ss2;   
    u32int cr3;   
    u32int eip;   
    u32int eflags;
    u32int eax;
    u32int ecx;
    u32int edx;
    u32int ebx;
    u32int esp;
    u32int ebp;
    u32int esi;
    u32int edi;
    u32int es;         // The value to load into ES when we change to kernel mode.
    u32int cs;         // The value to load into CS when we change to kernel mode.
    u32int ss;         // The value to load into SS when we change to kernel mode.
    u32int ds;         // The value to load into DS when we change to kernel mode.
    u32int fs;         // The value to load into FS when we change to kernel mode.
    u32int gs;         // The value to load into GS when we change to kernel mode.
    u32int ldt;        // Unused...
    u16int trap;
    u16int iomap_base;

} __attribute__((packed));

typedef struct tss_entry tss_entry_t;

// A struct describing an interrupt gate.
struct idt_entry
{
    u16int base_low;            // The lower 16 bits of the address to jump to when this interrupt fires.
    u16int selector;            // Kernel segment selector.
    u8int  always0;             // This must always be zero.
    u8int  flags;               // More flags. See documentation.
    u16int base_high;           // The upper 16 bits of the address to jump to.
} __attribute__((packed));

typedef struct idt_entry idt_entry_t;

/*
 * A struct describing a pointer to an array of interrupt handlers.
 * This is in a format suitable for giving to 'lidt'.
 */
struct idt_ptr
{
    u16int limit;
    u32int base;                // The address of the first element in our idt_entry_t array.
} __attribute__((packed));

typedef struct idt_ptr idt_ptr_t;

#endif
