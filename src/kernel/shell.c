#include "shell.h"
#include "keyboard.h"
#include "console.h"
#include "utils.h"
#include "pmm.h"
#include "process.h"
#include "fs.h"
#include "heap.h"

#include "arch/arch_platform.h"
#include "hal/system.h"
#include "hal/cpu.h"

#define MAX_CMD_LEN 256
static char cmd_buffer[MAX_CMD_LEN];
static int cmd_index = 0;

void print_prompt(void) {
    kprintf("\nAdrOS $> ");
}

void execute_command(char* cmd) {
    kprintf("\n");
    
    if (strcmp(cmd, "help") == 0) {
        kprintf("Available commands:\n");
        kprintf("  help        - Show this list\n");
        kprintf("  clear       - Clear screen\n");
        kprintf("  ls          - List files (Dummy)\n");
        kprintf("  cat <file>  - Read file content\n");
        kprintf("  mem         - Show memory stats\n");
        kprintf("  panic       - Trigger kernel panic\n");
        kprintf("  ring3       - Run usermode syscall test\n");
        kprintf("  reboot      - Restart system\n");
        kprintf("  sleep <num> - Sleep for N ticks\n");
        kprintf("  dmesg       - Show kernel log buffer\n");
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
    else if (strcmp(cmd, "clear") == 0) {
        kprintf("\033[2J\033[1;1H");
    }
    else if (strncmp(cmd, "sleep ", 6) == 0) {
        int ticks = atoi(cmd + 6);
        kprintf("Sleeping for %s ticks...\n", cmd + 6);
        process_sleep(ticks);
        kprintf("Woke up!\n");
    }
    else if (strcmp(cmd, "mem") == 0) {
        kprintf("Memory Stats:\n");
        kprintf("  Total RAM: [TODO] MB\n");
    }
    else if (strcmp(cmd, "panic") == 0) {
        hal_cpu_disable_interrupts();
        for(;;) {
            hal_cpu_idle();
        }
    }
    else if (strcmp(cmd, "ring3") == 0) {
        arch_platform_usermode_test_start();
    }
    else if (strcmp(cmd, "dmesg") == 0) {
        char dmesg_buf[4096];
        size_t n = klog_read(dmesg_buf, sizeof(dmesg_buf));
        if (n > 0) {
            console_write(dmesg_buf);
        } else {
            kprintf("(empty)\n");
        }
    }
    else if (strcmp(cmd, "reboot") == 0) {
        hal_system_reboot();
    }
    else if (strlen(cmd) > 0) {
        kprintf("Unknown command: %s\n", cmd);
    }
    
    print_prompt();
}

void shell_callback(char c) {
    if (c == '\n') {
        cmd_buffer[cmd_index] = 0;
        execute_command(cmd_buffer);
        cmd_index = 0;
    }
    else if (c == '\b') {
        if (cmd_index > 0) {
            cmd_index--;
            console_write("\b \b");
        }
    }
    else {
        if (cmd_index < MAX_CMD_LEN - 1) {
            cmd_buffer[cmd_index++] = c;
            char str[2] = { c, 0 };
            console_write(str);
        }
    }
}

void shell_init(void) {
    kprintf("[SHELL] Starting Shell...\n");
    cmd_index = 0;
    keyboard_set_callback(shell_callback);
    print_prompt();
}
