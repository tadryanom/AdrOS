#include "timer.h"
#include "idt.h"
#include "io.h"
#include "uart_console.h"
#include "process.h" // For schedule()

static uint32_t tick = 0;

void timer_callback(struct registers* regs) {
    (void)regs;
    tick++;
    
    // Every 100 ticks (approx 1 sec), print a dot just to show life
    if (tick % 100 == 0) {
        // uart_print("."); // Commented out to not pollute shell
    }
    
    // PREEMPTION!
    // Force a task switch
    schedule();
}

void timer_init(uint32_t frequency) {
    uart_print("[TIMER] Initializing PIT...\n");
    
    // Register Timer Callback (IRQ 0 -> Int 32)
    register_interrupt_handler(32, timer_callback);
    
    // The value we send to the PIT divisor is the value to divide it's input clock
    // (1193180 Hz) by, to get our required frequency.
    uint32_t divisor = 1193180 / frequency;
    
    // Send the command byte.
    // 0x36 = 0011 0110
    // Channel 0 | Access lo/hi byte | Mode 3 (Square Wave) | 16-bit binary
    outb(0x43, 0x36);
    
    // Split divisor into low and high bytes
    uint8_t l = (uint8_t)(divisor & 0xFF);
    uint8_t h = (uint8_t)( (divisor>>8) & 0xFF );
    
    // Send the frequency divisor.
    outb(0x40, l);
    outb(0x40, h);
}
