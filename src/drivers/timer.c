#include "timer.h"
#include "idt.h"
#include "io.h"
#include "uart_console.h"
#include "process.h" 

static uint32_t tick = 0;

uint32_t get_tick_count(void) {
    return tick;
}

void timer_callback(struct registers* regs) {
    (void)regs;
    tick++;
    
    // Check if anyone needs to wake up
    process_wake_check(tick);
    
    // Every 100 ticks (approx 2 sec at 50Hz), print dot
    if (tick % 100 == 0) {
        // uart_print(".");
    }
    
    schedule();
}

void timer_init(uint32_t frequency) {
    uart_print("[TIMER] Initializing PIT...\n");
    register_interrupt_handler(32, timer_callback);
    
    uint32_t divisor = 1193180 / frequency;
    outb(0x43, 0x36);
    uint8_t l = (uint8_t)(divisor & 0xFF);
    uint8_t h = (uint8_t)( (divisor>>8) & 0xFF );
    outb(0x40, l);
    outb(0x40, h);
}
