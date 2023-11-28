/* 
 * Initialises the GDT and IDT, and defines the
 * default ISR and IRQ handler.
 * Based on code from Bran's and JamesM's kernel development tutorials.
 * Rewritten by Tulio Mendes to AdrOS Kernel
 */

#include <descriptors.h>
#include <isr.h>
#include <string.h>
#include <system.h>

//#define GDT_ENTRIES 6
#define GDT_ENTRIES 5

// Lets us access our ASM functions from our C code.
extern void gdt_flush (u32int);
//extern void tss_flush (void);
extern void idt_flush (u32int);

// Internal function prototypes.
static void init_gdt (void);
static void gdt_set_gate (s32int, u32int, u32int, u8int, u8int);
//static void write_tss (s32int, u16int, u32int);
static void init_idt (void);
static void idt_set_gate (u8int, u32int, u16int, u8int);

gdt_entry_t gdt[GDT_ENTRIES];
gdt_ptr_t   gdt_ptr;
//tss_entry_t tss;
idt_entry_t idt[IDT_ENTRIES];
idt_ptr_t   idt_ptr;

// Extern the ISR handler array so we can nullify them on startup.
extern isr_t interrupt_handlers[];

/*
 * Initialisation routine - zeroes all the interrupt service routines,
 * initialises the GDT and IDT.
 */
void init_descriptors (void)
{
    init_gdt(); // Initialise the global descriptor table.
    init_idt(); // Initialise the interrupt descriptor table.
    memset((u8int *)&interrupt_handlers, 0, sizeof(isr_t) * IDT_ENTRIES); // Nullify all the interrupt handlers.
}

/*void set_kernel_stack (u32int stack)
{
    tss.esp0 = stack;
}*/

static void init_gdt (void)
{
    gdt_ptr.limit = (sizeof(gdt_entry_t) * GDT_ENTRIES) - 1;
    gdt_ptr.base  = (u32int)&gdt;

    gdt_set_gate(0, 0, 0, 0, 0);                // Null segment
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF); // Code segment
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF); // Data segment
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF); // User mode code segment
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF); // User mode data segment
    //write_tss(5, 0x10, 0x0);

    gdt_flush((u32int)&gdt_ptr);
    //tss_flush();
}

// Set the value of one GDT entry.
static void gdt_set_gate (s32int index, u32int base, u32int limit, u8int access, u8int granularity)
{
    gdt[index].base_low    = (base & 0xFFFF);
    gdt[index].base_middle = (base >> 16) & 0xFF;
    gdt[index].base_high   = (base >> 24) & 0xFF;

    gdt[index].limit_low   = (limit & 0xFFFF);
    gdt[index].granularity = (limit >> 16) & 0x0F;
    
    gdt[index].granularity |= granularity & 0xF0;
    gdt[index].access      = access;
}

// Initialise our task state segment structure.
/*static void write_tss (s32int index, u16int ss0, u32int esp0)
{
    // Firstly, let's compute the base and limit of our entry into the GDT.
    u32int base  = (u32int)&tss;
    u32int limit = base + sizeof(tss);

    // Now, add our TSS descriptor's address to the GDT.
    gdt_set_gate(index, base, limit, 0xE9, 0x00);

    // Ensure the descriptor is initially zero.
    memset((u8int *)&tss, 0, sizeof(tss));

    tss.ss0  = ss0;  // Set the kernel stack segment.
    tss.esp0 = esp0; // Set the kernel stack pointer.*/

    /*
     * Here we set the cs, ss, ds, es, fs and gs entries in the TSS. These specify what 
     * segments should be loaded when the processor switches to kernel mode. Therefore
     * they are just our normal kernel code/data segments - 0x08 and 0x10 respectively,
     * but with the last two bits set, making 0x0b and 0x13. The setting of these bits
     * sets the RPL (requested privilege level) to 3, meaning that this TSS can be used
     * to switch to kernel mode from ring 3.
     */
/*    tss.cs = 0x0b;     
    tss.ss = tss.ds = tss.es = tss.fs = tss.gs = 0x13;
}*/

static void init_idt (void)
{
    idt_ptr.limit = sizeof(idt_entry_t) * IDT_ENTRIES -1;
    idt_ptr.base  = (u32int)&idt;

    memset((u8int *)&idt, 0, sizeof(idt_entry_t) * IDT_ENTRIES);

    // Remap the irq table.
    outportb(0x20, 0x11);
    outportb(0xA0, 0x11);
    outportb(0x21, 0x20);
    outportb(0xA1, 0x28);
    outportb(0x21, 0x04);
    outportb(0xA1, 0x02);
    outportb(0x21, 0x01);
    outportb(0xA1, 0x01);
    outportb(0x21, 0x0);
    outportb(0xA1, 0x0);

    idt_set_gate( 0, (u32int)isr0 , 0x08, 0x8E);
    idt_set_gate( 1, (u32int)isr1 , 0x08, 0x8E);
    idt_set_gate( 2, (u32int)isr2 , 0x08, 0x8E);
    idt_set_gate( 3, (u32int)isr3 , 0x08, 0x8E);
    idt_set_gate( 4, (u32int)isr4 , 0x08, 0x8E);
    idt_set_gate( 5, (u32int)isr5 , 0x08, 0x8E);
    idt_set_gate( 6, (u32int)isr6 , 0x08, 0x8E);
    idt_set_gate( 7, (u32int)isr7 , 0x08, 0x8E);
    idt_set_gate( 8, (u32int)isr8 , 0x08, 0x8E);
    idt_set_gate( 9, (u32int)isr9 , 0x08, 0x8E);
    idt_set_gate(10, (u32int)isr10, 0x08, 0x8E);
    idt_set_gate(11, (u32int)isr11, 0x08, 0x8E);
    idt_set_gate(12, (u32int)isr12, 0x08, 0x8E);
    idt_set_gate(13, (u32int)isr13, 0x08, 0x8E);
    idt_set_gate(14, (u32int)isr14, 0x08, 0x8E);
    idt_set_gate(15, (u32int)isr15, 0x08, 0x8E);
    idt_set_gate(16, (u32int)isr16, 0x08, 0x8E);
    idt_set_gate(17, (u32int)isr17, 0x08, 0x8E);
    idt_set_gate(18, (u32int)isr18, 0x08, 0x8E);
    idt_set_gate(19, (u32int)isr19, 0x08, 0x8E);
    idt_set_gate(20, (u32int)isr20, 0x08, 0x8E);
    idt_set_gate(21, (u32int)isr21, 0x08, 0x8E);
    idt_set_gate(22, (u32int)isr22, 0x08, 0x8E);
    idt_set_gate(23, (u32int)isr23, 0x08, 0x8E);
    idt_set_gate(24, (u32int)isr24, 0x08, 0x8E);
    idt_set_gate(25, (u32int)isr25, 0x08, 0x8E);
    idt_set_gate(26, (u32int)isr26, 0x08, 0x8E);
    idt_set_gate(27, (u32int)isr27, 0x08, 0x8E);
    idt_set_gate(28, (u32int)isr28, 0x08, 0x8E);
    idt_set_gate(29, (u32int)isr29, 0x08, 0x8E);
    idt_set_gate(30, (u32int)isr30, 0x08, 0x8E);
    idt_set_gate(31, (u32int)isr31, 0x08, 0x8E);
    idt_set_gate(IRQ0, (u32int)irq0, 0x08, 0x8E);
    idt_set_gate(IRQ1, (u32int)irq1, 0x08, 0x8E);
    idt_set_gate(IRQ2, (u32int)irq2, 0x08, 0x8E);
    idt_set_gate(IRQ3, (u32int)irq3, 0x08, 0x8E);
    idt_set_gate(IRQ4, (u32int)irq4, 0x08, 0x8E);
    idt_set_gate(IRQ5, (u32int)irq5, 0x08, 0x8E);
    idt_set_gate(IRQ6, (u32int)irq6, 0x08, 0x8E);
    idt_set_gate(IRQ7, (u32int)irq7, 0x08, 0x8E);
    idt_set_gate(IRQ8, (u32int)irq8, 0x08, 0x8E);
    idt_set_gate(IRQ9, (u32int)irq9, 0x08, 0x8E);
    idt_set_gate(IRQ10, (u32int)irq10, 0x08, 0x8E);
    idt_set_gate(IRQ11, (u32int)irq11, 0x08, 0x8E);
    idt_set_gate(IRQ12, (u32int)irq12, 0x08, 0x8E);
    idt_set_gate(IRQ13, (u32int)irq13, 0x08, 0x8E);
    idt_set_gate(IRQ14, (u32int)irq14, 0x08, 0x8E);
    idt_set_gate(IRQ15, (u32int)irq15, 0x08, 0x8E);
    idt_set_gate(128, (u32int)isr128, 0x08, 0x8E);

    idt_flush((u32int)&idt_ptr);
}

static void idt_set_gate (u8int index, u32int base, u16int selector, u8int flags)
{
    idt[index].base_low  = base & 0xFFFF;
    idt[index].base_high = (base >> 16) & 0xFFFF;

    idt[index].selector  = selector;
    idt[index].always0   = 0;
    idt[index].flags     = flags  | 0x60;
}
