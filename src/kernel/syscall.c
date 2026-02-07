#include "syscall.h"
#include "idt.h"
#include "fs.h"
#include "heap.h"
#include "keyboard.h"
#include "keyboard.h"
#include "process.h"
#include "uart_console.h"
#include "uaccess.h"

#include <stddef.h>

struct file {
    fs_node_t* node;
    uint32_t offset;
    uint32_t flags;
};

static int fd_alloc(struct file* f) {
    if (!current_process || !f) return -1;

    for (int fd = 3; fd < PROCESS_MAX_FILES; fd++) {
        if (current_process->files[fd] == NULL) {
            current_process->files[fd] = f;
            return fd;
        }
    }
    return -1;
}

static struct file* fd_get(int fd) {
    if (!current_process) return NULL;
    if (fd < 0 || fd >= PROCESS_MAX_FILES) return NULL;
    return current_process->files[fd];
}

static int fd_close(int fd) {
    if (!current_process) return -1;
    if (fd < 0 || fd >= PROCESS_MAX_FILES) return -1;

    struct file* f = current_process->files[fd];
    if (!f) return -1;
    current_process->files[fd] = NULL;
    kfree(f);
    return 0;
}

static int syscall_open_impl(const char* user_path, uint32_t flags) {
    (void)flags;
    if (!user_path) return -1;

    char path[128];
    for (size_t i = 0; i < sizeof(path); i++) {
        if (copy_from_user(&path[i], &user_path[i], 1) < 0) {
            return -1;
        }
        if (path[i] == 0) break;
        if (i + 1 == sizeof(path)) {
            path[sizeof(path) - 1] = 0;
            break;
        }
    }

    fs_node_t* node = vfs_lookup(path);
    if (!node) return -1;
    if ((node->flags & FS_FILE) == 0) return -1;

    struct file* f = (struct file*)kmalloc(sizeof(*f));
    if (!f) return -1;
    f->node = node;
    f->offset = 0;
    f->flags = 0;

    int fd = fd_alloc(f);
    if (fd < 0) {
        kfree(f);
        return -1;
    }
    return fd;
}

static int syscall_read_impl(int fd, void* user_buf, uint32_t len) {
    if (len > 1024 * 1024) return -1;
    if (user_range_ok(user_buf, (size_t)len) == 0) return -1;

    if (fd == 0) {
        char kbuf[256];
        uint32_t chunk = len;
        if (chunk > sizeof(kbuf)) chunk = (uint32_t)sizeof(kbuf);
        int rd = keyboard_read_blocking(kbuf, chunk);
        if (rd <= 0) return -1;
        if (copy_to_user(user_buf, kbuf, (size_t)rd) < 0) return -1;
        return rd;
    }

    if (fd == 1 || fd == 2) return -1;

    struct file* f = fd_get(fd);
    if (!f || !f->node) return -1;

    uint8_t kbuf[256];
    uint32_t total = 0;
    while (total < len) {
        uint32_t chunk = len - total;
        if (chunk > sizeof(kbuf)) chunk = (uint32_t)sizeof(kbuf);

        uint32_t rd = vfs_read(f->node, f->offset, chunk, kbuf);
        if (rd == 0) break;

        if (copy_to_user((uint8_t*)user_buf + total, kbuf, rd) < 0) {
            return -1;
        }

        f->offset += rd;
        total += rd;

        if (rd < chunk) break;
    }

    return (int)total;
}

static void syscall_handler(struct registers* regs) {
    uint32_t syscall_no = regs->eax;

    if (syscall_no == SYSCALL_WRITE) {
        uint32_t fd = regs->ebx;
        const char* buf = (const char*)regs->ecx;
        uint32_t len = regs->edx;

        if (fd != 1 && fd != 2) {
            regs->eax = (uint32_t)-1;
            return;
        }

        if (len > 1024 * 1024) {
            regs->eax = (uint32_t)-1;
            return;
        }

        if (user_range_ok(buf, (size_t)len) == 0) {
            regs->eax = (uint32_t)-1;
            return;
        }

        char kbuf[256];
        uint32_t remaining = len;
        uintptr_t up = (uintptr_t)buf;

        while (remaining) {
            uint32_t chunk = remaining;
            if (chunk > sizeof(kbuf)) chunk = (uint32_t)sizeof(kbuf);

            if (copy_from_user(kbuf, (const void*)up, (size_t)chunk) < 0) {
                regs->eax = (uint32_t)-1;
                return;
            }

            for (uint32_t i = 0; i < chunk; i++) {
                uart_put_char(kbuf[i]);
            }

            up += chunk;
            remaining -= chunk;
        }

        regs->eax = len;
        return;
    }

    if (syscall_no == SYSCALL_GETPID) {
        regs->eax = 0;
        return;
    }

    if (syscall_no == SYSCALL_OPEN) {
        const char* path = (const char*)regs->ebx;
        uint32_t flags = regs->ecx;
        regs->eax = (uint32_t)syscall_open_impl(path, flags);
        return;
    }

    if (syscall_no == SYSCALL_READ) {
        int fd = (int)regs->ebx;
        void* buf = (void*)regs->ecx;
        uint32_t len = regs->edx;
        regs->eax = (uint32_t)syscall_read_impl(fd, buf, len);
        return;
    }

    if (syscall_no == SYSCALL_CLOSE) {
        int fd = (int)regs->ebx;
        regs->eax = (uint32_t)fd_close(fd);
        return;
    }

    if (syscall_no == SYSCALL_EXIT) {
        uart_print("[USER] exit()\n");
        for(;;) {
            __asm__ volatile("cli; hlt");
        }
    }

    regs->eax = (uint32_t)-1;
}

void syscall_init(void) {
#if defined(__i386__)
    register_interrupt_handler(128, syscall_handler);
#endif
}
