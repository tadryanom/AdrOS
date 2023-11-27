/* 
 * Initialises the PIT, and handles clock updates.
 * Based on code from JamesM's kernel development tutorials.
 * Rewritten by Tulio Mendes to AdrOS Kernel
 */

#include <timer.h>
#include <isr.h>
#include <system.h>
#include <task.h>
#include <stdio.h>

u32int tick = 0;
u64int rtc_tick = 0;

// Internal function prototypes.
static void timer_callback (registers_t *);
static void rtc_callback (registers_t *);
static u8int * convert_digit (u8int *, u8int);

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

void init_rtc (void)
{
    register_interrupt_handler(IRQ8, &rtc_callback);
	outportb(0x70, 0x8B);
	u8int prev = inportb(0x71);
	outportb(0x70, 0x8B);
	outportb(0x71, prev | 0x40);
}

void sleep_ms (u32int ms)
{
    u64int eticks = tick + ms;
    while (tick < (u32int)eticks){}
}

static void timer_callback (registers_t *regs)
{
    (void)regs;
    tick++;
    //switch_task();
}

static void rtc_callback (registers_t *regs)
{
    (void)regs;
	u8int buf[3];
	u8int day_of_week[][4] = {"Sun\0", "Mon\0", "Tue\0", "Wed\0", "Thu\0", "Fri\0", "Sat\0"};
    rtc_tick++;
	if (rtc_tick > 1000) {
		outportb(0x70, 0x06);
		u8int ddd = inportb(0x71);
		printf("RTC: %s ", day_of_week[ddd - 1]);
		outportb(0x70, 0x09);
		u8int yy = inportb(0x71);
        printf("20%s/", convert_digit(buf, yy));
		outportb(0x70, 0x08);
		u8int mm = inportb(0x71);
		printf("%s/", convert_digit(buf, mm));
		outportb(0x70, 0x07);
		u8int dd = inportb(0x71);
		printf("%s ", convert_digit(buf, dd));
		outportb(0x70, 0x04);
		u8int hh = inportb(0x71);
		printf("%s:", convert_digit(buf, hh));
		outportb(0x70, 0x02);
		u8int nn = inportb(0x71);
		printf("%s:", convert_digit(buf, nn));
		outportb(0x70, 0x00);
		u8int ss = inportb(0x71);
		printf("%s\n", convert_digit(buf, ss));
		rtc_tick = 0;
    }
	outportb(0x70, 0x0C);
	inportb(0x71);
}

static u8int * convert_digit (u8int * buf, u8int digit)
{
	if        ((u8int)digit <= 0x09) {
		buf[0] = 0x30;
		buf[1] = digit + 0x30;
		buf[2] = 0x00;
	} else if (((u8int)digit >= 0x10) && ((u8int)digit <= 0x19)) {
		buf[0] = 0x31;
		buf[1] = digit + 0x20;
		buf[2] = 0x00;
	} else if (((u8int)digit >= 0x20) && ((u8int)digit <= 0x29)) {
		buf[0] = 0x32;
		buf[1] = digit + 0x10;
		buf[2] = 0x00;
	} else if (((u8int)digit >= 0x30) && ((u8int)digit <= 0x39)) {
		buf[0] = 0x33;
		buf[1] = digit;
		buf[2] = 0x00;
	} else if (((u8int)digit >= 0x40) && ((u8int)digit <= 0x49)) {
		buf[0] = 0x34;
		buf[1] = digit - 0x10;
		buf[2] = 0x00;
	} else if (((u8int)digit >= 0x50) && ((u8int)digit <= 0x59)) {
		buf[0] = 0x35;
		buf[1] = digit - 0x20;
		buf[2] = 0x00;
	}
	return buf;
}
