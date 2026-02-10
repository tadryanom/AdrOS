#include "kconsole.h"
#include "console.h"
#include "utils.h"
#include "hal/system.h"
#include "hal/cpu.h"

#define KCMD_MAX 128

static void kconsole_help(void) {
    kprintf("kconsole commands:\n");
    kprintf("  help   - Show this list\n");
    kprintf("  dmesg  - Show kernel log buffer\n");
    kprintf("  reboot - Restart system\n");
    kprintf("  halt   - Halt the CPU\n");
}

static void kconsole_exec(const char* cmd) {
    if (strcmp(cmd, "help") == 0) {
        kconsole_help();
    }
    else if (strcmp(cmd, "dmesg") == 0) {
        char buf[4096];
        size_t n = klog_read(buf, sizeof(buf));
        if (n > 0) {
            console_write(buf);
        } else {
            kprintf("(empty)\n");
        }
    }
    else if (strcmp(cmd, "reboot") == 0) {
        hal_system_reboot();
    }
    else if (strcmp(cmd, "halt") == 0) {
        kprintf("System halted.\n");
        hal_cpu_disable_interrupts();
        for (;;) hal_cpu_idle();
    }
    else if (cmd[0] != '\0') {
        kprintf("unknown command: %s\n", cmd);
    }
}

void kconsole_enter(void) {
    kprintf("\n*** AdrOS Kernel Console (kconsole) ***\n");
    kprintf("Type 'help' for available commands.\n");

    char line[KCMD_MAX];
    int pos = 0;

    for (;;) {
        kprintf("kconsole> ");

        pos = 0;
        while (pos < KCMD_MAX - 1) {
            int ch = kgetc();
            if (ch < 0) continue;

            if (ch == '\n' || ch == '\r') {
                console_write("\n");
                break;
            }

            if (ch == '\b' || ch == 127) {
                if (pos > 0) {
                    pos--;
                    console_write("\b \b");
                }
                continue;
            }

            line[pos++] = (char)ch;
            char echo[2] = { (char)ch, 0 };
            console_write(echo);
        }
        line[pos] = '\0';

        kconsole_exec(line);
    }
}
