#include <system.h>

void outportb (u16int port, u8int data)
{
    asm volatile ("outb %1, %0" : : "dN" (port), "a" (data));
}

u8int inportb (u16int port)
{
    u8int ret;
    asm volatile("inb %1, %0" : "=a" (ret) : "dN" (port));
    return ret;
}

u16int inportw(u16int port)
{
    u16int ret;
    asm volatile ("inw %1, %0" : "=a" (ret) : "dN" (port));
    return ret;
}
