#include <system.h>
#include <stdio.h>

u8int inportb (u16int port)
{
    u8int ret;
    asm volatile("inb %1, %0" : "=a" (ret) : "dN" (port));
    return ret;
}

u16int inportw (u16int port)
{
    u16int ret;
    asm volatile ("inw %1, %0" : "=a" (ret) : "dN" (port));
    return ret;
}

void outportb (u16int port, u8int data)
{
    asm volatile ("outb %1, %0" : : "dN" (port), "a" (data));
}

void outportw (u16int port, u16int data)
{
    asm volatile ("outw %1, %0" : : "dN" (port), "a" (data));
}

void panic (const s8int *message, const s8int *file, u32int line)
{
    // We encountered a massive problem and have to stop.
    asm volatile("cli"); // Disable interrupts.

    printf("PANIC(%s) at %s:%d\n", message, file, line);
    // Halt by going into an infinite loop.
    for(;;);
}

void panic_assert (const s8int *file, u32int line, const s8int *desc)
{
    // An assertion failed, and we have to panic.
    asm volatile("cli"); // Disable interrupts.

    printf("ASSERTION-FAILED(%s) at %s:%d\n", desc, file, line);
    // Halt by going into an infinite loop.
    for(;;);
}
