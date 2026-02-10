#include "kconsole.h"
#include "console.h"
#include "utils.h"
#include "fs.h"
#include "heap.h"
#include "process.h"
#include "arch/arch_platform.h"
#include "hal/system.h"
#include "hal/cpu.h"

#define KCMD_MAX 128

static void kconsole_help(void) {
    kprintf("kconsole commands:\n");
    kprintf("  help        - Show this list\n");
    kprintf("  clear       - Clear screen\n");
    kprintf("  ls          - List files\n");
    kprintf("  cat <file>  - Read file content\n");
    kprintf("  mem         - Show memory stats\n");
    kprintf("  dmesg       - Show kernel log buffer\n");
    kprintf("  sleep <num> - Sleep for N ticks\n");
    kprintf("  ring3       - Run usermode syscall test\n");
    kprintf("  reboot      - Restart system\n");
    kprintf("  halt        - Halt the CPU\n");
}

static void kconsole_exec(const char* cmd) {
    if (strcmp(cmd, "help") == 0) {
        kconsole_help();
    }
    else if (strcmp(cmd, "clear") == 0) {
        kprintf("\033[2J\033[1;1H");
    }
    else if (strcmp(cmd, "ls") == 0) {
        if (!fs_root) {
            kprintf("No filesystem mounted.\n");
        } else {
            kprintf("Filesystem Mounted (InitRD).\n");
            kprintf("Try: cat test.txt\n");
        }
    }
    else if (strncmp(cmd, "cat ", 4) == 0) {
        if (!fs_root) {
            kprintf("No filesystem mounted.\n");
        } else {
            const char* fname = cmd + 4;
            fs_node_t* file = NULL;
            if (fname[0] == '/') {
                file = vfs_lookup(fname);
            } else {
                char abs[132];
                abs[0] = '/';
                abs[1] = 0;
                strcpy(abs + 1, fname);
                file = vfs_lookup(abs);
            }
            if (file) {
                kprintf("Reading %s...\n", fname);
                uint8_t* buf = (uint8_t*)kmalloc(file->length + 1);
                if (buf) {
                    uint32_t sz = vfs_read(file, 0, file->length, buf);
                    buf[sz] = 0;
                    kprintf("%s\n", (char*)buf);
                    kfree(buf);
                } else {
                    kprintf("OOM: File too big for heap.\n");
                }
            } else {
                kprintf("File not found.\n");
            }
        }
    }
    else if (strcmp(cmd, "mem") == 0) {
        kprintf("Memory Stats:\n");
        kprintf("  Total RAM: [TODO] MB\n");
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
    else if (strncmp(cmd, "sleep ", 6) == 0) {
        int ticks = atoi(cmd + 6);
        kprintf("Sleeping for %s ticks...\n", cmd + 6);
        process_sleep(ticks);
        kprintf("Woke up!\n");
    }
    else if (strcmp(cmd, "ring3") == 0) {
        arch_platform_usermode_test_start();
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
