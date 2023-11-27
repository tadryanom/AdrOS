/* 
 * Initialises the PIT, and handles clock updates.
 * Based on code from Bran's and JamesM's kernel development tutorials.
 * Rewritten by Tulio Mendes to AdrOS Kernel
 */

#include <timer.h>
#include <isr.h>
#include <system.h>

u32int tick = 0;
u32int prev_tick = 0;

// Internal function prototypes.
static void timer_callback (registers_t *);

void init_timer (u32int frequency)
{
    // Firstly, register our timer callback.
    register_interrupt_handler(IRQ0, &timer_callback);

    /*
     * The value we send to the PIT is the value to divide it's input clock
     * (1193180 Hz) by, to get our required frequency. Important to note is
     * that the divisor must be small enough to fit into 16-bits.
     */
    u32int divisor = 1193180 / frequency;

    // Send the command byte.
    outportb(0x43, 0x36);

    // Divisor has to be sent byte-wise, so split here into upper/lower bytes.
    u8int l = (u8int)(divisor & 0xFF);
    u8int h = (u8int)((divisor>>8) & 0xFF );

    // Send the frequency divisor.
    outportb(0x40, l);
    outportb(0x40, h);
}

void sleep_ms (u32int ms)
{
    prev_tick = tick;
    while (tick - prev_tick < ms){}
}

static void timer_callback (registers_t *regs)
{
    (void)regs;
    tick++;
    //switch_task();
}
