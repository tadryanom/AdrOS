#include "syscall.h"
#include "idt.h"
#include "fs.h"
#include "heap.h"
#include "tty.h"
#include "process.h"
#include "uart_console.h"
#include "uaccess.h"

#include "errno.h"
#include "stat.h"

#include "hal/cpu.h"

#include <stddef.h>

struct file {
    fs_node_t* node;
    uint32_t offset;
    uint32_t flags;
};

static int stat_from_node(const fs_node_t* node, struct stat* st) {
    if (!node || !st) return -EFAULT;

    st->st_ino = node->inode;
    st->st_nlink = 1;
    st->st_size = node->length;

    uint32_t mode = 0;
    if (node->flags == FS_DIRECTORY) mode |= S_IFDIR;
    else if (node->flags == FS_CHARDEVICE) mode |= S_IFCHR;
    else mode |= S_IFREG;
    st->st_mode = mode;
    return 0;
}

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

static int syscall_stat_impl(const char* user_path, struct stat* user_st) {
    if (!user_path || !user_st) return -EFAULT;
    if (user_range_ok(user_st, sizeof(*user_st)) == 0) return -EFAULT;

    char path[128];
    for (size_t i = 0; i < sizeof(path); i++) {
        if (copy_from_user(&path[i], &user_path[i], 1) < 0) {
            return -EFAULT;
        }
        if (path[i] == 0) break;
        if (i + 1 == sizeof(path)) {
            path[sizeof(path) - 1] = 0;
            break;
        }
    }

    fs_node_t* node = vfs_lookup(path);
    if (!node) return -ENOENT;

    struct stat st;
    int rc = stat_from_node(node, &st);
    if (rc < 0) return rc;
    if (copy_to_user(user_st, &st, sizeof(st)) < 0) return -EFAULT;
    return 0;
}

static int syscall_fstat_impl(int fd, struct stat* user_st) {
    if (!user_st) return -EFAULT;
    if (user_range_ok(user_st, sizeof(*user_st)) == 0) return -EFAULT;

    struct file* f = fd_get(fd);
    if (!f || !f->node) return -EBADF;

    struct stat st;
    int rc = stat_from_node(f->node, &st);
    if (rc < 0) return rc;
    if (copy_to_user(user_st, &st, sizeof(st)) < 0) return -EFAULT;
    return 0;
}

static int syscall_lseek_impl(int fd, int32_t offset, int whence) {
    if (fd == 0 || fd == 1 || fd == 2) return -ESPIPE;

    struct file* f = fd_get(fd);
    if (!f || !f->node) return -EBADF;
    if (f->node->flags != FS_FILE) return -ESPIPE;

    int64_t base = 0;
    if (whence == 0) {
        base = 0;
    } else if (whence == 1) {
        base = (int64_t)f->offset;
    } else if (whence == 2) {
        base = (int64_t)f->node->length;
    } else {
        return -EINVAL;
    }

    int64_t noff = base + (int64_t)offset;
    if (noff < 0) return -EINVAL;
    if (noff > (int64_t)f->node->length) return -EINVAL;

    f->offset = (uint32_t)noff;
    return (int)f->offset;
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
    if (node->flags != FS_FILE) return -1;

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
        return tty_read(user_buf, len);
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

        regs->eax = (uint32_t)tty_write(buf, len);
        return;
    }

    if (syscall_no == SYSCALL_GETPID) {
        regs->eax = current_process ? current_process->pid : 0;
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
        int status = (int)regs->ebx;

        for (int fd = 3; fd < PROCESS_MAX_FILES; fd++) {
            if (current_process && current_process->files[fd]) {
                (void)fd_close(fd);
            }
        }

        process_exit_notify(status);

        hal_cpu_enable_interrupts();
        schedule();
        for(;;) {
            __asm__ volatile("cli; hlt");
        }
    }

    if (syscall_no == SYSCALL_WAITPID) {
        int pid = (int)regs->ebx;
        int* user_status = (int*)regs->ecx;
        uint32_t options = regs->edx;
        (void)options;

        if (user_status && user_range_ok(user_status, sizeof(int)) == 0) {
            regs->eax = (uint32_t)-1;
            return;
        }

        int status = 0;
        int retpid = process_waitpid(pid, &status);
        if (retpid < 0) {
            regs->eax = (uint32_t)-1;
            return;
        }

        if (user_status) {
            if (copy_to_user(user_status, &status, sizeof(status)) < 0) {
                regs->eax = (uint32_t)-1;
                return;
            }
        }

        regs->eax = (uint32_t)retpid;
        return;
    }

    if (syscall_no == SYSCALL_SPAWN) {
        int pid = process_spawn_test_child();
        regs->eax = (pid < 0) ? (uint32_t)-1 : (uint32_t)pid;
        return;
    }

    if (syscall_no == SYSCALL_LSEEK) {
        int fd = (int)regs->ebx;
        int32_t off = (int32_t)regs->ecx;
        int whence = (int)regs->edx;
        regs->eax = (uint32_t)syscall_lseek_impl(fd, off, whence);
        return;
    }

    if (syscall_no == SYSCALL_FSTAT) {
        int fd = (int)regs->ebx;
        struct stat* st = (struct stat*)regs->ecx;
        regs->eax = (uint32_t)syscall_fstat_impl(fd, st);
        return;
    }

    if (syscall_no == SYSCALL_STAT) {
        const char* path = (const char*)regs->ebx;
        struct stat* st = (struct stat*)regs->ecx;
        regs->eax = (uint32_t)syscall_stat_impl(path, st);
        return;
    }

    regs->eax = (uint32_t)-1;
}

void syscall_init(void) {
#if defined(__i386__)
    register_interrupt_handler(128, syscall_handler);
#endif
}
