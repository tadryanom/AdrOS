#include "hal/system.h"

#if defined(__i386__) || defined(__x86_64__)
#include "io.h"
#include <stdint.h>

void hal_system_reboot(void) {
    uint8_t good = 0x02;
    while (good & 0x02) good = inb(0x64);
    outb(0x64, 0xFE);
}
#else
void hal_system_reboot(void) {
}
#endif
