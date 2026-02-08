#include "shell.h"
#include "keyboard.h"
#include "uart_console.h"
#include "utils.h"
#include "pmm.h"
#include "vga_console.h"
#include "process.h" 
#include "fs.h"
#include "heap.h"

#include "hal/system.h"
#include "hal/cpu.h"

#define MAX_CMD_LEN 256
static char cmd_buffer[MAX_CMD_LEN];
static int cmd_index = 0;

void print_prompt(void) {
    uart_print("\nAdrOS $> ");
}

void execute_command(char* cmd) {
    uart_print("\n");
    
    if (strcmp(cmd, "help") == 0) {
        uart_print("Available commands:\n");
        uart_print("  help        - Show this list\n");
        uart_print("  clear       - Clear screen\n");
        uart_print("  ls          - List files (Dummy)\n");
        uart_print("  cat <file>  - Read file content\n");
        uart_print("  mem         - Show memory stats\n");
        uart_print("  panic       - Trigger kernel panic\n");
        uart_print("  ring3       - Run x86 ring3 syscall test\n");
        uart_print("  reboot      - Restart system\n");
        uart_print("  sleep <num> - Sleep for N ticks\n");
    } 
    else if (strcmp(cmd, "ls") == 0) {
        if (!fs_root) {
            uart_print("No filesystem mounted.\n");
        } else {
            uart_print("Filesystem Mounted (InitRD).\n");
            uart_print("Try: cat test.txt\n");
        }
    }
    else if (strncmp(cmd, "cat ", 4) == 0) {
        if (!fs_root) {
            uart_print("No filesystem mounted.\n");
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
                uart_print("Reading "); uart_print(fname); uart_print("...\n");
                uint8_t* buf = (uint8_t*)kmalloc(file->length + 1);
                if (buf) {
                    uint32_t sz = vfs_read(file, 0, file->length, buf);
                    buf[sz] = 0;
                    uart_print((char*)buf);
                    uart_print("\n");
                    kfree(buf);
                } else {
                    uart_print("OOM: File too big for heap.\n");
                }
            } else {
                uart_print("File not found.\n");
            }
        }
    }
    else if (strcmp(cmd, "clear") == 0) {
        uart_print("\033[2J\033[1;1H");
    }
    else if (strncmp(cmd, "sleep ", 6) == 0) {
        int ticks = atoi(cmd + 6);
        uart_print("Sleeping for ");
        uart_print(cmd + 6);
        uart_print(" ticks...\n");
        process_sleep(ticks);
        uart_print("Woke up!\n");
    }
    else if (strcmp(cmd, "mem") == 0) {
        uart_print("Memory Stats:\n");
        uart_print("  Total RAM: [TODO] MB\n");
    }
    else if (strcmp(cmd, "panic") == 0) {
#if defined(__i386__) || defined(__x86_64__)
        __asm__ volatile("cli");
        __asm__ volatile("ud2");
#else
        for(;;) {
            hal_cpu_idle();
        }
#endif
    }
    else if (strcmp(cmd, "ring3") == 0) {
#if defined(__i386__)
        extern void x86_usermode_test_start(void);
        x86_usermode_test_start();
#else
        uart_print("ring3 test only available on x86.\n");
#endif
    }
    else if (strcmp(cmd, "reboot") == 0) {
        hal_system_reboot();
    }
    else if (strlen(cmd) > 0) {
        uart_print("Unknown command: ");
        uart_print(cmd);
        uart_print("\n");
    }
    
    print_prompt();
}

void shell_callback(char c) {
    char str[2] = { c, 0 };
    
    if (c == '\n') {
        cmd_buffer[cmd_index] = 0;
        execute_command(cmd_buffer);
        cmd_index = 0;
    }
    else if (c == '\b') {
        if (cmd_index > 0) {
            cmd_index--;
            uart_print("\b \b");
        }
    }
    else {
        if (cmd_index < MAX_CMD_LEN - 1) {
            cmd_buffer[cmd_index++] = c;
            uart_print(str);
        }
    }
}

void shell_init(void) {
    uart_print("[SHELL] Starting Shell...\n");
    cmd_index = 0;
    keyboard_set_callback(shell_callback);
    print_prompt();
}
