#include "kconsole.h"
#include "console.h"
#include "vga_console.h"
#include "utils.h"
#include "fs.h"
#include "heap.h"
#include "pmm.h"
#include "process.h"
#include "keyboard.h"
#include "arch/arch_platform.h"
#include "hal/system.h"
#include "hal/cpu.h"

#define KCMD_MAX 128

static void kc_puts(const char* s) {
    console_write(s);
}

static void kconsole_help(void) {
    kc_puts("kconsole commands:\n");
    kc_puts("  help        - Show this list\n");
    kc_puts("  clear       - Clear screen\n");
    kc_puts("  ls [path]   - List files in directory\n");
    kc_puts("  cat <file>  - Read file content\n");
    kc_puts("  mem         - Show memory stats\n");
    kc_puts("  dmesg       - Show kernel log buffer\n");
    kc_puts("  reboot      - Restart system\n");
    kc_puts("  halt        - Halt the CPU\n");
}

static void kconsole_ls(const char* path) {
    fs_node_t* dir = NULL;

    if (!path || path[0] == '\0') {
        dir = fs_root;
    } else {
        dir = vfs_lookup(path);
    }

    if (!dir) {
        kprintf("ls: cannot access '%s': not found\n", path ? path : "/");
        return;
    }

    if (!dir->readdir) {
        kprintf("ls: not a directory\n");
        return;
    }

    uint32_t idx = 0;
    struct vfs_dirent ent;
    while (1) {
        int rc = dir->readdir(dir, &idx, &ent, sizeof(ent));
        if (rc != 0) break;
        kprintf("  %s\n", ent.d_name);
    }
}

static void kconsole_exec(const char* cmd) {
    if (strcmp(cmd, "help") == 0) {
        kconsole_help();
    }
    else if (strcmp(cmd, "clear") == 0) {
        vga_clear();
    }
    else if (strcmp(cmd, "ls") == 0) {
        kconsole_ls(NULL);
    }
    else if (strncmp(cmd, "ls ", 3) == 0) {
        kconsole_ls(cmd + 3);
    }
    else if (strncmp(cmd, "cat ", 4) == 0) {
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
            uint8_t* buf = (uint8_t*)kmalloc(file->length + 1);
            if (buf) {
                uint32_t sz = vfs_read(file, 0, file->length, buf);
                buf[sz] = 0;
                kprintf("%s\n", (char*)buf);
                kfree(buf);
            } else {
                kprintf("cat: out of memory\n");
            }
        } else {
            kprintf("cat: %s: not found\n", fname);
        }
    }
    else if (strcmp(cmd, "mem") == 0) {
        kprintf("Memory Stats:\n");
        pmm_print_stats();
    }
    else if (strcmp(cmd, "dmesg") == 0) {
        char buf[4096];
        size_t n = klog_read(buf, sizeof(buf));
        if (n > 0) {
            console_write(buf);
            console_write("\n");
        } else {
            kc_puts("(empty)\n");
        }
    }
    else if (strcmp(cmd, "reboot") == 0) {
        hal_system_reboot();
    }
    else if (strcmp(cmd, "halt") == 0) {
        kc_puts("System halted.\n");
        hal_cpu_disable_interrupts();
        for (;;) hal_cpu_idle();
    }
    else if (cmd[0] != '\0') {
        kprintf("unknown command: %s\n", cmd);
    }
}

void kconsole_enter(void) {
    keyboard_set_callback(0);

    kc_puts("\n*** AdrOS Kernel Console (kconsole) ***\n");
    kc_puts("Type 'help' for available commands.\n");

    char line[KCMD_MAX];
    int pos = 0;

    for (;;) {
        kc_puts("kconsole> ");

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

            if (ch < ' ' || ch > '~') continue;

            line[pos++] = (char)ch;
            char echo[2] = { (char)ch, 0 };
            console_write(echo);
        }
        line[pos] = '\0';

        kconsole_exec(line);
    }
}
