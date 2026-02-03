#include "shell.h"
#include "keyboard.h"
#include "uart_console.h"
#include "utils.h"
#include "pmm.h"
#include "vga_console.h"
#include "process.h"
#include "fs.h"

#define MAX_CMD_LEN 256
static char cmd_buffer[MAX_CMD_LEN];
static int cmd_index = 0;

// HACK: To list files since we lack readdir
extern int n_root_nodes;
extern fs_node_t* root_nodes;

void print_prompt(void) {
    uart_print("\nAdrOS $> ");
}

void execute_command(char* cmd) {
    uart_print("\n");
    
    if (strcmp(cmd, "help") == 0) {
        uart_print("Available commands:\n");
        uart_print("  help        - Show this list\n");
        uart_print("  clear       - Clear screen\n");
        uart_print("  mem         - Show memory stats\n");
        uart_print("  panic       - Trigger kernel panic\n");
        uart_print("  reboot      - Restart system\n");
        uart_print("  sleep <num> - Sleep for N ticks\n");
        uart_print("  ls          - List files\n");
        uart_print("  cat <file>  - Print file content\n");
    } 
    else if (strcmp(cmd, "ls") == 0) {
        if (!fs_root) {
            uart_print("No filesystem mounted.\n");
        } else {
            uart_print("Files:\n");
            for(int i=0; i<n_root_nodes; i++) {
                uart_print("  ");
                uart_print(root_nodes[i].name);
                uart_print("\n");
            }
        }
    }
    else if (strncmp(cmd, "cat ", 4) == 0) {
        if (!fs_root) {
            uart_print("No filesystem mounted.\n");
        } else {
            char* fname = cmd + 4;
            // Trim spaces?
            fs_node_t* file = NULL;
            if (fs_root->finddir)
                file = fs_root->finddir(fs_root, fname);
            
            if (file) {
                // Alloc buffer or use stack
                uint8_t buf[1024]; 
                // Cap read at 1023
                uint32_t len = file->length > 1023 ? 1023 : file->length;
                
                vfs_read(file, 0, len, buf);
                buf[len] = 0;
                uart_print((char*)buf);
                uart_print("\n");
            } else {
                uart_print("File not found.\n");
            }
        }
    }
    else if (strcmp(cmd, "clear") == 0) {
        // ANSI clear screen for UART
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
        // pmm_print_stats() is not impl yet, so let's fake it or add it
        uart_print("Memory Stats:\n");
        uart_print("  Total RAM: [TODO] MB\n");
        uart_print("  Used:      [TODO] KB\n");
        uart_print("  Free:      [TODO] KB\n");
    }
    else if (strcmp(cmd, "panic") == 0) {
        // Trigger Int 0 (Div by zero)
        int a = 1;
        int b = 0;
        int c = a / b;
        (void)c;
    }
    else if (strcmp(cmd, "reboot") == 0) {
        // 8042 keyboard controller reset command
        uint8_t good = 0x02;
        while (good & 0x02)
            good = inb(0x64);
        outb(0x64, 0xFE);
        
        // Triple Fault fallback
        // idt_set_gate(0, 0, 0, 0);
        // __asm__("int $0");
    }
    else if (strlen(cmd) > 0) {
        uart_print("Unknown command: ");
        uart_print(cmd);
        uart_print("\n");
    }
    
    print_prompt();
}

void shell_callback(char c) {
    // Echo to screen
    char str[2] = { c, 0 };
    
    if (c == '\n') {
        cmd_buffer[cmd_index] = 0; // Null terminate
        execute_command(cmd_buffer);
        cmd_index = 0;
    }
    else if (c == '\b') {
        if (cmd_index > 0) {
            cmd_index--;
            uart_print("\b \b"); // Backspace hack
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
