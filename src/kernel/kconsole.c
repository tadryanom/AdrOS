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

/* ---- Output helpers ---- */

static void kc_puts(const char* s) {
    console_write(s);
}

static void kc_putc(char c) {
    console_put_char(c);
}

/* ---- Command history ---- */

#define HIST_MAX 16
static char hist_buf[HIST_MAX][KCMD_MAX];
static int hist_head  = 0;  /* next write slot */
static int hist_count = 0;  /* entries stored */

static void hist_add(const char* line) {
    if (line[0] == '\0') return;
    /* Don't add duplicates of the last command */
    if (hist_count > 0) {
        int prev = (hist_head - 1 + HIST_MAX) % HIST_MAX;
        if (strcmp(hist_buf[prev], line) == 0) return;
    }
    strncpy(hist_buf[hist_head], line, KCMD_MAX - 1);
    hist_buf[hist_head][KCMD_MAX - 1] = '\0';
    hist_head = (hist_head + 1) % HIST_MAX;
    if (hist_count < HIST_MAX) hist_count++;
}

static const char* hist_get(int idx) {
    /* idx 0 = most recent, 1 = second most recent, etc. */
    if (idx < 0 || idx >= hist_count) return 0;
    int slot = (hist_head - 1 - idx + HIST_MAX) % HIST_MAX;
    return hist_buf[slot];
}

/* ---- Line editing helpers ---- */

static void kc_cursor_left(int n) {
    for (int i = 0; i < n; i++) kc_putc('\b');
}

static void kc_erase_line(char* buf, int* len, int* cur) {
    kc_cursor_left(*cur);
    for (int i = 0; i < *len; i++) kc_putc(' ');
    kc_cursor_left(*len);
    *len = 0;
    *cur = 0;
    buf[0] = '\0';
}

static void kc_replace_line(char* buf, int* len, int* cur, const char* text) {
    kc_erase_line(buf, len, cur);
    int nlen = (int)strlen(text);
    if (nlen >= KCMD_MAX) nlen = KCMD_MAX - 1;
    memcpy(buf, text, (size_t)nlen);
    buf[nlen] = '\0';
    *len = nlen;
    *cur = nlen;
    for (int i = 0; i < nlen; i++) kc_putc(buf[i]);
}

/* ---- Readline with VT100 escape parsing ---- */

static int kc_readline(char* buf, int maxlen) {
    int len = 0;
    int cur = 0;
    int hist_nav = -1;    /* -1 = editing current, 0+ = history index */
    char saved[KCMD_MAX]; /* saved current line when navigating history */
    saved[0] = '\0';

    enum { ST_NORMAL, ST_ESC, ST_CSI } state = ST_NORMAL;
    char csi_buf[8];
    int csi_len = 0;

    buf[0] = '\0';

    for (;;) {
        int ch = kgetc();
        if (ch < 0) continue;

        if (state == ST_ESC) {
            if (ch == '[') {
                state = ST_CSI;
                csi_len = 0;
            } else {
                state = ST_NORMAL;
            }
            continue;
        }

        if (state == ST_CSI) {
            if (ch >= '0' && ch <= '9' && csi_len < 7) {
                csi_buf[csi_len++] = (char)ch;
                continue;
            }
            csi_buf[csi_len] = '\0';
            state = ST_NORMAL;

            switch (ch) {
            case 'A': /* Up arrow — previous history */
                if (hist_nav + 1 < hist_count) {
                    if (hist_nav == -1) {
                        memcpy(saved, buf, (size_t)(len + 1));
                    }
                    hist_nav++;
                    kc_replace_line(buf, &len, &cur, hist_get(hist_nav));
                }
                break;
            case 'B': /* Down arrow — next history / restore */
                if (hist_nav > 0) {
                    hist_nav--;
                    kc_replace_line(buf, &len, &cur, hist_get(hist_nav));
                } else if (hist_nav == 0) {
                    hist_nav = -1;
                    kc_replace_line(buf, &len, &cur, saved);
                }
                break;
            case 'C': /* Right arrow */
                if (cur < len) {
                    kc_putc(buf[cur]);
                    cur++;
                }
                break;
            case 'D': /* Left arrow */
                if (cur > 0) {
                    cur--;
                    kc_putc('\b');
                }
                break;
            case 'H': /* Home */
                kc_cursor_left(cur);
                cur = 0;
                break;
            case 'F': /* End */
                for (int i = cur; i < len; i++) kc_putc(buf[i]);
                cur = len;
                break;
            case '~': { /* Delete (CSI 3 ~) */
                int param = 0;
                for (int i = 0; i < csi_len; i++)
                    param = param * 10 + (csi_buf[i] - '0');
                if (param == 3 && cur < len) {
                    memmove(&buf[cur], &buf[cur + 1], (size_t)(len - cur - 1));
                    len--;
                    buf[len] = '\0';
                    for (int i = cur; i < len; i++) kc_putc(buf[i]);
                    kc_putc(' ');
                    kc_cursor_left(len - cur + 1);
                }
                break;
            }
            default:
                break;
            }
            continue;
        }

        /* ST_NORMAL */
        if (ch == 0x1B) {
            state = ST_ESC;
            continue;
        }

        if (ch == '\n' || ch == '\r') {
            kc_putc('\n');
            buf[len] = '\0';
            return len;
        }

        if (ch == '\b' || ch == 127) {
            if (cur > 0) {
                cur--;
                memmove(&buf[cur], &buf[cur + 1], (size_t)(len - cur - 1));
                len--;
                buf[len] = '\0';
                kc_putc('\b');
                for (int i = cur; i < len; i++) kc_putc(buf[i]);
                kc_putc(' ');
                kc_cursor_left(len - cur + 1);
            }
            continue;
        }

        /* Ctrl-A = Home, Ctrl-E = End, Ctrl-U = kill line, Ctrl-K = kill to end */
        if (ch == 0x01) { /* Ctrl-A */
            kc_cursor_left(cur);
            cur = 0;
            continue;
        }
        if (ch == 0x05) { /* Ctrl-E */
            for (int i = cur; i < len; i++) kc_putc(buf[i]);
            cur = len;
            continue;
        }
        if (ch == 0x15) { /* Ctrl-U: kill whole line */
            kc_erase_line(buf, &len, &cur);
            continue;
        }
        if (ch == 0x0B) { /* Ctrl-K: kill to end of line */
            for (int i = cur; i < len; i++) kc_putc(' ');
            kc_cursor_left(len - cur);
            len = cur;
            buf[len] = '\0';
            continue;
        }

        if (ch < ' ' || ch > '~') continue;

        /* Insert printable character at cursor */
        if (len >= maxlen - 1) continue;
        memmove(&buf[cur + 1], &buf[cur], (size_t)(len - cur));
        buf[cur] = (char)ch;
        len++;
        buf[len] = '\0';
        for (int i = cur; i < len; i++) kc_putc(buf[i]);
        cur++;
        kc_cursor_left(len - cur);
    }
}

/* ---- Commands ---- */

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

/* ---- Main entry ---- */

void kconsole_enter(void) {
    keyboard_set_callback(0);

    kc_puts("\n[PANIC] Userspace init failed -- dropping to kconsole.\n");
    kc_puts("        Type 'help' for commands, 'reboot' to restart.\n\n");

    char line[KCMD_MAX];

    for (;;) {
        kc_puts("kconsole> ");
        kc_readline(line, KCMD_MAX);

        if (line[0] != '\0') {
            hist_add(line);
        }

        klog_set_suppress(1);
        kconsole_exec(line);
        klog_set_suppress(0);
    }
}
