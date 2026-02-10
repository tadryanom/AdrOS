#include "syscall.h"
#include "interrupts.h"
#include "fs.h"
#include "process.h"
#include "spinlock.h"
#include "uaccess.h"
#include "uart_console.h"
#include "utils.h"

#include "heap.h"
#include "tty.h"
#include "pty.h"
#include "diskfs.h"

#include "errno.h"
#include "shm.h"

#if defined(__i386__)
extern void x86_sysenter_init(void);
#endif
#include "elf.h"
#include "stat.h"
#include "vmm.h"
#include "pmm.h"
#include "timer.h"
#include "hal/mm.h"

#include "hal/cpu.h"

#include <stddef.h>

enum {
    O_NONBLOCK = 0x800,
    O_CLOEXEC  = 0x80000,
};

enum {
    FD_CLOEXEC = 1,
};

enum {
    FCNTL_F_DUPFD = 0,
    FCNTL_F_GETFD = 1,
    FCNTL_F_SETFD = 2,
    FCNTL_F_GETFL = 3,
    FCNTL_F_SETFL = 4,
};

enum {
    AT_FDCWD = -100,
};

static int path_resolve_user(const char* user_path, char* out, size_t out_sz);

#if defined(__i386__)
static const uint32_t SIGFRAME_MAGIC = 0x53494746U; // 'SIGF'
struct sigframe {
    uint32_t magic;
    struct registers saved;
};
#endif

static int fd_alloc(struct file* f);
static int fd_close(int fd);
static struct file* fd_get(int fd);

struct pollfd {
    int fd;
    int16_t events;
    int16_t revents;
};

enum {
    POLLIN = 0x0001,
    POLLOUT = 0x0004,
    POLLERR = 0x0008,
    POLLHUP = 0x0010,
};

static int poll_wait_kfds(struct pollfd* kfds, uint32_t nfds, int32_t timeout);

static int syscall_select_impl(uint32_t nfds,
                               uint64_t* user_readfds,
                               uint64_t* user_writefds,
                               uint64_t* user_exceptfds,
                               int32_t timeout) {
    if (nfds > 64U) return -EINVAL;
    if (user_exceptfds) return -EINVAL;

    uint64_t rmask = 0;
    uint64_t wmask = 0;
    if (user_readfds) {
        if (user_range_ok(user_readfds, sizeof(*user_readfds)) == 0) return -EFAULT;
        if (copy_from_user(&rmask, user_readfds, sizeof(rmask)) < 0) return -EFAULT;
    }
    if (user_writefds) {
        if (user_range_ok(user_writefds, sizeof(*user_writefds)) == 0) return -EFAULT;
        if (copy_from_user(&wmask, user_writefds, sizeof(wmask)) < 0) return -EFAULT;
    }

    struct pollfd kfds[64];
    uint32_t cnt = 0;
    for (uint32_t fd = 0; fd < nfds; fd++) {
        int16_t events = 0;
        if ((rmask >> fd) & 1U) events |= POLLIN;
        if ((wmask >> fd) & 1U) events |= POLLOUT;
        if (!events) continue;

        kfds[cnt].fd = (int)fd;
        kfds[cnt].events = events;
        kfds[cnt].revents = 0;
        cnt++;
    }

    if (cnt == 0) {
        if (user_readfds && copy_to_user(user_readfds, &rmask, sizeof(rmask)) < 0) return -EFAULT;
        if (user_writefds && copy_to_user(user_writefds, &wmask, sizeof(wmask)) < 0) return -EFAULT;
        return 0;
    }

    int rc = poll_wait_kfds(kfds, cnt, timeout);
    if (rc < 0) return rc;

    uint64_t r_out = 0;
    uint64_t w_out = 0;
    int ready = 0;

    for (uint32_t i = 0; i < cnt; i++) {
        uint32_t fd = (uint32_t)kfds[i].fd;
        if ((kfds[i].revents & POLLIN) && ((rmask >> fd) & 1U)) {
            r_out |= (1ULL << fd);
        }
        if ((kfds[i].revents & POLLOUT) && ((wmask >> fd) & 1U)) {
            w_out |= (1ULL << fd);
        }
    }

    uint64_t any = r_out | w_out;
    for (uint32_t fd = 0; fd < nfds; fd++) {
        if ((any >> fd) & 1ULL) ready++;
    }

    if (user_readfds && copy_to_user(user_readfds, &r_out, sizeof(r_out)) < 0) return -EFAULT;
    if (user_writefds && copy_to_user(user_writefds, &w_out, sizeof(w_out)) < 0) return -EFAULT;
    return ready;
}

static int execve_copy_user_str(char* out, size_t out_sz, const char* user_s) {
    if (!out || out_sz == 0 || !user_s) return -EFAULT;
    for (size_t i = 0; i < out_sz; i++) {
        if (copy_from_user(&out[i], &user_s[i], 1) < 0) return -EFAULT;
        if (out[i] == 0) return 0;
    }
    out[out_sz - 1] = 0;
    return 0;
}

static int execve_copy_user_ptr(const void* const* user_p, uintptr_t* out) {
    if (!out) return -EFAULT;
    if (!user_p) {
        *out = 0;
        return 0;
    }
    uintptr_t tmp = 0;
    if (copy_from_user(&tmp, user_p, sizeof(tmp)) < 0) return -EFAULT;
    *out = tmp;
    return 0;
}

static int syscall_fork_impl(struct registers* regs) {
    if (!regs) return -EINVAL;
    if (!current_process) return -EINVAL;

    uintptr_t src_as = hal_cpu_get_address_space() & ~(uintptr_t)0xFFFU;
    if (current_process->addr_space != src_as) {
        current_process->addr_space = src_as;
    }

    uintptr_t child_as = vmm_as_clone_user_cow(src_as);
    if (!child_as) return -ENOMEM;

    struct registers child_regs = *regs;
    child_regs.eax = 0;

    struct process* child = process_fork_create(child_as, &child_regs);
    if (!child) {
        vmm_as_destroy(child_as);
        return -ENOMEM;
    }

    child->heap_start = current_process->heap_start;
    child->heap_break = current_process->heap_break;

    for (int fd = 0; fd < PROCESS_MAX_FILES; fd++) {
        struct file* f = current_process->files[fd];
        if (!f) continue;
        __sync_fetch_and_add(&f->refcount, 1);
        child->files[fd] = f;
        child->fd_flags[fd] = current_process->fd_flags[fd];
    }

    return (int)child->pid;
}

struct pipe_state {
    uint8_t* buf;
    uint32_t cap;
    uint32_t rpos;
    uint32_t wpos;
    uint32_t count;
    uint32_t readers;
    uint32_t writers;
};

struct pipe_node {
    fs_node_t node;
    struct pipe_state* ps;
    uint32_t is_read_end;
};

static int poll_wait_kfds(struct pollfd* kfds, uint32_t nfds, int32_t timeout) {
    if (!kfds) return -EINVAL;
    if (nfds > 64U) return -EINVAL;

    // timeout semantics (minimal):
    //  - timeout == 0 : non-blocking
    //  - timeout  < 0 : block forever
    //  - timeout  > 0 : treated as "ticks" (best-effort)
    extern uint32_t get_tick_count(void);
    uint32_t start_tick = get_tick_count();

    for (;;) {
        int ready = 0;
        for (uint32_t i = 0; i < nfds; i++) {
            kfds[i].revents = 0;
            int fd = kfds[i].fd;
            if (fd < 0) continue;

            struct file* f = fd_get(fd);
            if (!f || !f->node) {
                kfds[i].revents |= POLLERR;
                continue;
            }

            fs_node_t* n = f->node;

            // Pipes (identified by node name prefix).
            if (n->name[0] == 'p' && n->name[1] == 'i' && n->name[2] == 'p' && n->name[3] == 'e' && n->name[4] == ':') {
                struct pipe_node* pn = (struct pipe_node*)n;
                struct pipe_state* ps = pn->ps;
                if (!ps) {
                    kfds[i].revents |= POLLERR;
                } else if (pn->is_read_end) {
                    if ((kfds[i].events & POLLIN) && (ps->count > 0 || ps->writers == 0)) {
                        kfds[i].revents |= POLLIN;
                        if (ps->writers == 0) kfds[i].revents |= POLLHUP;
                    }
                } else {
                    if (ps->readers == 0) {
                        if (kfds[i].events & POLLOUT) kfds[i].revents |= POLLERR;
                    } else {
                        uint32_t free = ps->cap - ps->count;
                        if ((kfds[i].events & POLLOUT) && free > 0) {
                            kfds[i].revents |= POLLOUT;
                        }
                    }
                }
            } else if (n->flags == FS_CHARDEVICE) {
                // devfs devices: inode 2=/dev/null, 3=/dev/tty
                if (n->inode == 2) {
                    if (kfds[i].events & POLLIN) kfds[i].revents |= POLLIN | POLLHUP;
                    if (kfds[i].events & POLLOUT) kfds[i].revents |= POLLOUT;
                } else if (n->inode == 3) {
                    if ((kfds[i].events & POLLIN) && tty_can_read()) kfds[i].revents |= POLLIN;
                    if ((kfds[i].events & POLLOUT) && tty_can_write()) kfds[i].revents |= POLLOUT;
                } else if (pty_is_master_ino(n->inode)) {
                    int pi = pty_ino_to_idx(n->inode);
                    if ((kfds[i].events & POLLIN) && pty_master_can_read_idx(pi)) kfds[i].revents |= POLLIN;
                    if ((kfds[i].events & POLLOUT) && pty_master_can_write_idx(pi)) kfds[i].revents |= POLLOUT;
                } else if (pty_is_slave_ino(n->inode)) {
                    int pi = pty_ino_to_idx(n->inode);
                    if ((kfds[i].events & POLLIN) && pty_slave_can_read_idx(pi)) kfds[i].revents |= POLLIN;
                    if ((kfds[i].events & POLLOUT) && pty_slave_can_write_idx(pi)) kfds[i].revents |= POLLOUT;
                }
            } else {
                // Regular files are always readable/writable (best-effort).
                if (kfds[i].events & POLLIN) kfds[i].revents |= POLLIN;
                if (kfds[i].events & POLLOUT) kfds[i].revents |= POLLOUT;
            }

            if (kfds[i].revents) ready++;
        }

        if (ready) return ready;
        if (timeout == 0) return 0;

        if (timeout > 0) {
            uint32_t now = get_tick_count();
            uint32_t elapsed = now - start_tick;
            if (elapsed >= (uint32_t)timeout) return 0;
        }

        process_sleep(1);
    }
}

static int syscall_poll_impl(struct pollfd* user_fds, uint32_t nfds, int32_t timeout) {
    if (!user_fds) return -EFAULT;
    if (nfds > 64U) return -EINVAL;
    if (user_range_ok(user_fds, sizeof(struct pollfd) * (size_t)nfds) == 0) return -EFAULT;

    struct pollfd kfds[64];
    if (copy_from_user(kfds, user_fds, sizeof(struct pollfd) * (size_t)nfds) < 0) return -EFAULT;

    int rc = poll_wait_kfds(kfds, nfds, timeout);
    if (rc < 0) return rc;

    if (copy_to_user(user_fds, kfds, sizeof(struct pollfd) * (size_t)nfds) < 0) return -EFAULT;
    return rc;
}

static uint32_t pipe_read(fs_node_t* n, uint32_t offset, uint32_t size, uint8_t* buffer) {
    (void)offset;
    struct pipe_node* pn = (struct pipe_node*)n;
    if (!pn || !pn->ps || !buffer) return 0;
    if (!pn->is_read_end) return 0;

    struct pipe_state* ps = pn->ps;
    if (size == 0) return 0;

    uint32_t to_read = size;
    if (to_read > ps->count) to_read = ps->count;

    for (uint32_t i = 0; i < to_read; i++) {
        buffer[i] = ps->buf[ps->rpos];
        ps->rpos++;
        if (ps->rpos == ps->cap) ps->rpos = 0;
    }
    ps->count -= to_read;
    return to_read;
}

static uint32_t pipe_write(fs_node_t* n, uint32_t offset, uint32_t size, const uint8_t* buffer) {
    (void)offset;
    struct pipe_node* pn = (struct pipe_node*)n;
    if (!pn || !pn->ps || !buffer) return 0;
    if (pn->is_read_end) return 0;

    struct pipe_state* ps = pn->ps;
    if (size == 0) return 0;
    if (ps->readers == 0) return 0;

    uint32_t free = ps->cap - ps->count;
    uint32_t to_write = size;
    if (to_write > free) to_write = free;

    for (uint32_t i = 0; i < to_write; i++) {
        ps->buf[ps->wpos] = buffer[i];
        ps->wpos++;
        if (ps->wpos == ps->cap) ps->wpos = 0;
    }
    ps->count += to_write;
    return to_write;
}

static void pipe_close(fs_node_t* n) {
    struct pipe_node* pn = (struct pipe_node*)n;
    if (!pn || !pn->ps) {
        if (pn) kfree(pn);
        return;
    }

    if (pn->is_read_end) {
        if (pn->ps->readers) pn->ps->readers--;
    } else {
        if (pn->ps->writers) pn->ps->writers--;
    }

    struct pipe_state* ps = pn->ps;
    kfree(pn);

    if (ps->readers == 0 && ps->writers == 0) {
        if (ps->buf) kfree(ps->buf);
        kfree(ps);
    }
}

static int pipe_node_create(struct pipe_state* ps, int is_read_end, fs_node_t** out_node) {
    if (!ps || !out_node) return -EINVAL;
    struct pipe_node* pn = (struct pipe_node*)kmalloc(sizeof(*pn));
    if (!pn) return -ENOMEM;
    memset(pn, 0, sizeof(*pn));

    pn->ps = ps;
    pn->is_read_end = is_read_end ? 1U : 0U;
    pn->node.flags = FS_FILE;
    pn->node.length = 0;
    pn->node.open = NULL;
    pn->node.finddir = NULL;
    pn->node.close = pipe_close;
    if (pn->is_read_end) {
        strcpy(pn->node.name, "pipe:r");
        pn->node.read = pipe_read;
        pn->node.write = NULL;
        ps->readers++;
    } else {
        strcpy(pn->node.name, "pipe:w");
        pn->node.read = NULL;
        pn->node.write = pipe_write;
        ps->writers++;
    }

    *out_node = &pn->node;
    return 0;
}

static int pipe_create_kfds(int kfds[2]) {
    if (!kfds) return -EINVAL;
    struct pipe_state* ps = (struct pipe_state*)kmalloc(sizeof(*ps));
    if (!ps) return -ENOMEM;
    memset(ps, 0, sizeof(*ps));
    ps->cap = 512;
    ps->buf = (uint8_t*)kmalloc(ps->cap);
    if (!ps->buf) {
        kfree(ps);
        return -ENOMEM;
    }

    fs_node_t* rnode = NULL;
    fs_node_t* wnode = NULL;
    if (pipe_node_create(ps, 1, &rnode) < 0) {
        kfree(ps->buf);
        kfree(ps);
        return -ENOMEM;
    }
    if (pipe_node_create(ps, 0, &wnode) < 0) {
        vfs_close(rnode);
        return -ENOMEM;
    }

    struct file* rf = (struct file*)kmalloc(sizeof(*rf));
    struct file* wf = (struct file*)kmalloc(sizeof(*wf));
    if (!rf || !wf) {
        if (rf) kfree(rf);
        if (wf) kfree(wf);
        vfs_close(rnode);
        vfs_close(wnode);
        return -ENOMEM;
    }
    memset(rf, 0, sizeof(*rf));
    memset(wf, 0, sizeof(*wf));
    rf->node = rnode;
    rf->refcount = 1;
    wf->node = wnode;
    wf->refcount = 1;

    int rfd = fd_alloc(rf);
    if (rfd < 0) {
        kfree(rf);
        kfree(wf);
        vfs_close(rnode);
        vfs_close(wnode);
        return -EMFILE;
    }

    int wfd = fd_alloc(wf);
    if (wfd < 0) {
        (void)fd_close(rfd);
        kfree(wf);
        vfs_close(wnode);
        return -EMFILE;
    }

    kfds[0] = rfd;
    kfds[1] = wfd;
    return 0;
}

static int syscall_pipe_impl(int* user_fds) {
    if (!user_fds) return -EFAULT;
    if (user_range_ok(user_fds, sizeof(int) * 2) == 0) return -EFAULT;

    int kfds[2];
    int rc = pipe_create_kfds(kfds);
    if (rc < 0) return rc;

    if (copy_to_user(user_fds, kfds, sizeof(kfds)) < 0) {
        (void)fd_close(kfds[0]);
        (void)fd_close(kfds[1]);
        return -EFAULT;
    }
    return 0;
}

static int syscall_pipe2_impl(int* user_fds, uint32_t flags) {
    if (!user_fds) return -EFAULT;
    if (user_range_ok(user_fds, sizeof(int) * 2) == 0) return -EFAULT;

    int kfds[2];
    int rc = pipe_create_kfds(kfds);
    if (rc < 0) return rc;
    if (!current_process) return -ECHILD;

    if (kfds[0] >= 0 && kfds[0] < PROCESS_MAX_FILES && current_process->files[kfds[0]]) {
        current_process->files[kfds[0]]->flags = flags & ~O_CLOEXEC;
    }
    if (kfds[1] >= 0 && kfds[1] < PROCESS_MAX_FILES && current_process->files[kfds[1]]) {
        current_process->files[kfds[1]]->flags = flags & ~O_CLOEXEC;
    }
    if (flags & O_CLOEXEC) {
        if (kfds[0] >= 0 && kfds[0] < PROCESS_MAX_FILES) current_process->fd_flags[kfds[0]] = FD_CLOEXEC;
        if (kfds[1] >= 0 && kfds[1] < PROCESS_MAX_FILES) current_process->fd_flags[kfds[1]] = FD_CLOEXEC;
    }

    if (copy_to_user(user_fds, kfds, sizeof(kfds)) < 0) {
        (void)fd_close(kfds[0]);
        (void)fd_close(kfds[1]);
        return -EFAULT;
    }

    return 0;
}

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

static int fd_alloc_from(int start_fd, struct file* f) {
    if (!current_process || !f) return -EINVAL;
    if (start_fd < 0) start_fd = 0;
    if (start_fd >= PROCESS_MAX_FILES) return -EINVAL;

    for (int fd = start_fd; fd < PROCESS_MAX_FILES; fd++) {
        if (current_process->files[fd] == NULL) {
            current_process->files[fd] = f;
            return fd;
        }
    }
    return -EMFILE;
}

static int fd_alloc(struct file* f) {
    if (!current_process || !f) return -EINVAL;

    for (int fd = 3; fd < PROCESS_MAX_FILES; fd++) {
        if (current_process->files[fd] == NULL) {
            current_process->files[fd] = f;
            return fd;
        }
    }
    return -EMFILE;
}

static struct file* fd_get(int fd) {
    if (!current_process) return NULL;
    if (fd < 0 || fd >= PROCESS_MAX_FILES) return NULL;
    return current_process->files[fd];
}

static int fd_close(int fd) {
    if (!current_process) return -EBADF;
    if (fd < 0 || fd >= PROCESS_MAX_FILES) return -EBADF;

    struct file* f = current_process->files[fd];
    if (!f) return -EBADF;
    current_process->files[fd] = NULL;

    if (__sync_sub_and_fetch(&f->refcount, 1) == 0) {
        if (f->node) {
            vfs_close(f->node);
        }
        kfree(f);
    }
    return 0;
}

static int syscall_dup_impl(int oldfd) {
    struct file* f = fd_get(oldfd);
    if (!f) return -EBADF;
    __sync_fetch_and_add(&f->refcount, 1);
    int newfd = fd_alloc_from(0, f);
    if (newfd < 0) {
        __sync_sub_and_fetch(&f->refcount, 1);
        return -EMFILE;
    }
    return newfd;
}

static int syscall_execve_impl(struct registers* regs, const char* user_path, const char* const* user_argv, const char* const* user_envp) {
    if (!regs || !user_path) return -EFAULT;

    enum {
        EXECVE_MAX_ARGC = 32,
        EXECVE_MAX_ENVC = 32,
        EXECVE_MAX_STR  = 128,
    };

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

    // Snapshot argv/envp into kernel buffers (before switching addr_space).
    char (*kargv)[EXECVE_MAX_STR] = (char(*)[EXECVE_MAX_STR])kmalloc((size_t)EXECVE_MAX_ARGC * (size_t)EXECVE_MAX_STR);
    char (*kenvp)[EXECVE_MAX_STR] = (char(*)[EXECVE_MAX_STR])kmalloc((size_t)EXECVE_MAX_ENVC * (size_t)EXECVE_MAX_STR);
    int argc = 0;
    int envc = 0;
    int ret = 0;

    if (!kargv || !kenvp) {
        ret = -ENOMEM;
        goto out;
    }

    if (user_argv) {
        for (int i = 0; i < EXECVE_MAX_ARGC; i++) {
            uintptr_t up = 0;
            int rc = execve_copy_user_ptr((const void* const*)&user_argv[i], &up);
            if (rc < 0) { ret = rc; goto out; }
            if (up == 0) break;
            rc = execve_copy_user_str(kargv[i], sizeof(kargv[i]), (const char*)up);
            if (rc < 0) { ret = rc; goto out; }
            argc++;
        }
    }

    if (user_envp) {
        for (int i = 0; i < EXECVE_MAX_ENVC; i++) {
            uintptr_t up = 0;
            int rc = execve_copy_user_ptr((const void* const*)&user_envp[i], &up);
            if (rc < 0) { ret = rc; goto out; }
            if (up == 0) break;
            rc = execve_copy_user_str(kenvp[i], sizeof(kenvp[i]), (const char*)up);
            if (rc < 0) { ret = rc; goto out; }
            envc++;
        }
    }

    // Distinguish ENOENT early.
    fs_node_t* node = vfs_lookup(path);
    if (!node) { ret = -ENOENT; goto out; }

    uintptr_t entry = 0;
    uintptr_t user_sp = 0;
    uintptr_t new_as = 0;
    uintptr_t heap_brk = 0;
    if (elf32_load_user_from_initrd(path, &entry, &user_sp, &new_as, &heap_brk) != 0) {
        ret = -EINVAL;
        goto out;
    }
    const size_t user_stack_size = 0x1000U;

    if ((size_t)((argc + 1) + (envc + 1)) * sizeof(uintptr_t) + (size_t)argc * EXECVE_MAX_STR + (size_t)envc * EXECVE_MAX_STR + 64U > user_stack_size) { vmm_as_destroy(new_as); ret = -E2BIG; goto out; }

    uintptr_t old_as = current_process ? current_process->addr_space : 0;
    if (!current_process) {
        vmm_as_destroy(new_as);
        ret = -EINVAL;
        goto out;
    }

    current_process->addr_space = new_as;
    current_process->heap_start = heap_brk;
    current_process->heap_break = heap_brk;
    vmm_as_activate(new_as);

    // Build a minimal initial user stack: argc, argv pointers, envp pointers, strings.
    // The loader returns a fresh stack top (user_sp). We'll pack strings below it.
    uintptr_t sp = user_sp;
    sp &= ~(uintptr_t)0xF;
    const uintptr_t sp_base = user_sp - user_stack_size;

    uintptr_t argv_ptrs_va[EXECVE_MAX_ARGC + 1];
    uintptr_t envp_ptrs_va[EXECVE_MAX_ENVC + 1];

    for (int i = envc - 1; i >= 0; i--) {
        size_t len = strlen(kenvp[i]) + 1;
        if (sp - len < sp_base) { vmm_as_activate(old_as); current_process->addr_space = old_as; vmm_as_destroy(new_as); ret = -E2BIG; goto out; }
        sp -= len;
        memcpy((void*)sp, kenvp[i], len);
        envp_ptrs_va[i] = sp;
    }
    envp_ptrs_va[envc] = 0;

    for (int i = argc - 1; i >= 0; i--) {
        size_t len = strlen(kargv[i]) + 1;
        if (sp - len < sp_base) { vmm_as_activate(old_as); current_process->addr_space = old_as; vmm_as_destroy(new_as); ret = -E2BIG; goto out; }
        sp -= len;
        memcpy((void*)sp, kargv[i], len);
        argv_ptrs_va[i] = sp;
    }
    argv_ptrs_va[argc] = 0;

    sp &= ~(uintptr_t)0xF;

    // Push envp[] pointers
    sp -= (uintptr_t)(sizeof(uintptr_t) * (envc + 1));
    memcpy((void*)sp, envp_ptrs_va, sizeof(uintptr_t) * (envc + 1));
    uintptr_t envp_va = sp;

    // Push argv[] pointers
    sp -= (uintptr_t)(sizeof(uintptr_t) * (argc + 1));
    memcpy((void*)sp, argv_ptrs_va, sizeof(uintptr_t) * (argc + 1));
    uintptr_t argv_va = sp;

    // Push argc
    sp -= sizeof(uint32_t);
    *(uint32_t*)sp = (uint32_t)argc;

    (void)argv_va;
    (void)envp_va;

    for (int i = 0; i < PROCESS_MAX_FILES; i++) {
        if (current_process->fd_flags[i] & FD_CLOEXEC) {
            (void)fd_close(i);
            current_process->fd_flags[i] = 0;
        }
    }

    if (old_as && old_as != new_as) {
        vmm_as_destroy(old_as);
    }

    regs->eip = (uint32_t)entry;
    regs->useresp = (uint32_t)sp;
    regs->eax = 0;
    ret = 0;
    goto out;

out:
    if (kargv) kfree(kargv);
    if (kenvp) kfree(kenvp);
    return ret;
}

static int syscall_dup2_impl(int oldfd, int newfd) {
    if (newfd < 0 || newfd >= PROCESS_MAX_FILES) return -EBADF;
    struct file* f = fd_get(oldfd);
    if (!f) return -EBADF;
    if (oldfd == newfd) return newfd;

    if (current_process && current_process->files[newfd]) {
        (void)fd_close(newfd);
    }

    __sync_fetch_and_add(&f->refcount, 1);
    current_process->files[newfd] = f;
    return newfd;
}

static int syscall_dup3_impl(int oldfd, int newfd, uint32_t flags) {
    // Minimal: accept only flags==0 for now.
    if (flags != 0) return -EINVAL;
    if (newfd < 0 || newfd >= PROCESS_MAX_FILES) return -EBADF;
    if (oldfd == newfd) return -EINVAL;
    struct file* f = fd_get(oldfd);
    if (!f) return -EBADF;

    if (current_process && current_process->files[newfd]) {
        (void)fd_close(newfd);
    }

    __sync_fetch_and_add(&f->refcount, 1);
    current_process->files[newfd] = f;
    return newfd;
}

static int syscall_stat_impl(const char* user_path, struct stat* user_st) {
    if (!user_path || !user_st) return -EFAULT;
    if (user_range_ok(user_st, sizeof(*user_st)) == 0) return -EFAULT;

    char path[128];
    int prc = path_resolve_user(user_path, path, sizeof(path));
    if (prc < 0) return prc;

    fs_node_t* node = vfs_lookup(path);
    if (!node) return -ENOENT;

    struct stat st;
    int rc = stat_from_node(node, &st);
    if (rc < 0) return rc;
    if (copy_to_user(user_st, &st, sizeof(st)) < 0) return -EFAULT;
    return 0;
}

static int syscall_fstatat_impl(int dirfd, const char* user_path, struct stat* user_st, uint32_t flags) {
    (void)flags;
    if (dirfd != AT_FDCWD) return -ENOSYS;
    return syscall_stat_impl(user_path, user_st);
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
    if (!user_path) return -EFAULT;

    char path[128];
    int prc = path_resolve_user(user_path, path, sizeof(path));
    if (prc < 0) return prc;

    fs_node_t* node = NULL;
    if (path[0] == '/' && path[1] == 'd' && path[2] == 'i' && path[3] == 's' && path[4] == 'k' && path[5] == '/') {
        // With hierarchical diskfs, /disk may contain directories.
        // Use diskfs_open_file only when creation/truncation is requested.
        if ((flags & 0x40U) != 0U || (flags & 0x200U) != 0U) {
            const char* rel = path + 6;
            if (rel[0] == 0) return -ENOENT;
            int rc = diskfs_open_file(rel, flags, &node);
            if (rc < 0) return rc;
        } else {
            node = vfs_lookup(path);
            if (!node) return -ENOENT;
        }
    } else {
        node = vfs_lookup(path);
        if (!node) return -ENOENT;
    }

    struct file* f = (struct file*)kmalloc(sizeof(*f));
    if (!f) return -ENOMEM;
    f->node = node;
    f->offset = 0;
    f->flags = flags;
    f->refcount = 1;

    int fd = fd_alloc(f);
    if (fd < 0) {
        kfree(f);
        return -EMFILE;
    }
    if ((flags & O_CLOEXEC) && current_process) {
        current_process->fd_flags[fd] = FD_CLOEXEC;
    }
    return fd;
}

static int syscall_openat_impl(int dirfd, const char* user_path, uint32_t flags, uint32_t mode) {
    (void)mode;
    if (dirfd != AT_FDCWD) return -ENOSYS;
    return syscall_open_impl(user_path, flags);
}

static int syscall_fcntl_impl(int fd, int cmd, uint32_t arg) {
    struct file* f = fd_get(fd);
    if (!f) return -EBADF;

    if (cmd == FCNTL_F_GETFD) {
        if (!current_process) return 0;
        return (int)current_process->fd_flags[fd];
    }
    if (cmd == FCNTL_F_SETFD) {
        if (!current_process) return -EINVAL;
        current_process->fd_flags[fd] = (uint8_t)(arg & FD_CLOEXEC);
        return 0;
    }
    if (cmd == FCNTL_F_GETFL) {
        return (int)f->flags;
    }
    if (cmd == FCNTL_F_SETFL) {
        uint32_t keep = f->flags & ~O_NONBLOCK;
        uint32_t set = arg & O_NONBLOCK;
        f->flags = keep | set;
        return 0;
    }
    return -EINVAL;
}

static int path_is_absolute(const char* p) {
    return p && p[0] == '/';
}

static void path_normalize_inplace(char* s) {
    if (!s) return;
    if (s[0] == 0) {
        strcpy(s, "/");
        return;
    }

    // Phase 1: split into components, resolve '.' and '..'
    char tmp[128];
    // Stack of component start offsets within tmp
    size_t comp_start[32];
    int depth = 0;
    size_t w = 0;

    const char* p = s;
    int absolute = (*p == '/');
    if (absolute) {
        tmp[w++] = '/';
        while (*p == '/') p++;
    }

    while (*p != 0) {
        // Extract next component
        const char* seg = p;
        while (*p != 0 && *p != '/') p++;
        size_t seg_len = (size_t)(p - seg);
        while (*p == '/') p++;

        if (seg_len == 1 && seg[0] == '.') {
            continue; // skip '.'
        }

        if (seg_len == 2 && seg[0] == '.' && seg[1] == '.') {
            // Go up one level
            if (depth > 0) {
                depth--;
                w = comp_start[depth];
            }
            continue;
        }

        // Record start of this component
        if (depth < 32) {
            comp_start[depth++] = w;
        }

        // Append separator if needed
        if (w > 1 || (w == 1 && tmp[0] != '/')) {
            if (w + 1 < sizeof(tmp)) tmp[w++] = '/';
        }

        // Append component
        for (size_t i = 0; i < seg_len && w + 1 < sizeof(tmp); i++) {
            tmp[w++] = seg[i];
        }
    }

    // Handle empty result
    if (w == 0) {
        tmp[w++] = '/';
    }

    // Remove trailing slash (except root)
    while (w > 1 && tmp[w - 1] == '/') {
        w--;
    }

    tmp[w] = 0;
    strcpy(s, tmp);
}

static int path_resolve_user(const char* user_path, char* out, size_t out_sz) {
    if (!out || out_sz == 0) return -EINVAL;
    out[0] = 0;
    if (!user_path) return -EFAULT;

    char in[128];
    for (size_t i = 0; i < sizeof(in); i++) {
        if (copy_from_user(&in[i], &user_path[i], 1) < 0) {
            return -EFAULT;
        }
        if (in[i] == 0) break;
        if (i + 1 == sizeof(in)) {
            in[sizeof(in) - 1] = 0;
            break;
        }
    }

    if (path_is_absolute(in)) {
        // bounded copy
        size_t i = 0;
        while (in[i] != 0 && i + 1 < out_sz) {
            out[i] = in[i];
            i++;
        }
        out[i] = 0;
        path_normalize_inplace(out);
        return 0;
    }

    const char* base = (current_process && current_process->cwd[0]) ? current_process->cwd : "/";
    size_t w = 0;
    if (strcmp(base, "/") == 0) {
        if (out_sz < 2) return -ENAMETOOLONG;
        out[w++] = '/';
    } else {
        for (size_t i = 0; base[i] != 0 && w + 1 < out_sz; i++) {
            out[w++] = base[i];
        }
        if (w + 1 < out_sz) out[w++] = '/';
    }

    for (size_t i = 0; in[i] != 0 && w + 1 < out_sz; i++) {
        out[w++] = in[i];
    }
    out[w] = 0;
    path_normalize_inplace(out);
    return 0;
}

static int syscall_chdir_impl(const char* user_path) {
    if (!current_process) return -EINVAL;
    char path[128];
    int rc = path_resolve_user(user_path, path, sizeof(path));
    if (rc < 0) return rc;

    fs_node_t* n = vfs_lookup(path);
    if (!n) return -ENOENT;
    if (n->flags != FS_DIRECTORY) return -ENOTDIR;
    strcpy(current_process->cwd, path);
    return 0;
}

static int syscall_getcwd_impl(char* user_buf, uint32_t size) {
    if (!current_process) return -EINVAL;
    if (!user_buf) return -EFAULT;
    if (size == 0) return -EINVAL;
    if (user_range_ok(user_buf, (size_t)size) == 0) return -EFAULT;

    const char* cwd = current_process->cwd[0] ? current_process->cwd : "/";
    uint32_t need = (uint32_t)strlen(cwd) + 1U;
    if (need > size) return -ERANGE;
    if (copy_to_user(user_buf, cwd, need) < 0) return -EFAULT;
    return 0;
}

static int syscall_mkdir_impl(const char* user_path) {
    if (!user_path) return -EFAULT;

    char path[128];
    int prc = path_resolve_user(user_path, path, sizeof(path));
    if (prc < 0) return prc;

    if (path[0] == '/' && path[1] == 'd' && path[2] == 'i' && path[3] == 's' && path[4] == 'k' && path[5] == '/') {
        const char* rel = path + 6;
        if (rel[0] == 0) return -EINVAL;
        return diskfs_mkdir(rel);
    }

    return -ENOSYS;
}

static int syscall_getdents_impl(int fd, void* user_buf, uint32_t len) {
    if (len == 0) return 0;
    if (!user_buf) return -EFAULT;
    if (user_range_ok(user_buf, (size_t)len) == 0) return -EFAULT;

    struct file* f = fd_get(fd);
    if (!f || !f->node) return -EBADF;
    if (f->node->flags != FS_DIRECTORY) return -ENOTDIR;
    if (!f->node->readdir) return -ENOSYS;

    uint8_t kbuf[256];
    uint32_t klen = len;
    if (klen > (uint32_t)sizeof(kbuf)) klen = (uint32_t)sizeof(kbuf);

    uint32_t idx = f->offset;
    int rc = f->node->readdir(f->node, &idx, kbuf, klen);
    if (rc < 0) return rc;
    if (rc == 0) return 0;

    if (copy_to_user(user_buf, kbuf, (uint32_t)rc) < 0) return -EFAULT;
    f->offset = idx;
    return rc;
}

static int syscall_unlink_impl(const char* user_path) {
    if (!user_path) return -EFAULT;

    char path[128];
    int prc = path_resolve_user(user_path, path, sizeof(path));
    if (prc < 0) return prc;

    if (path[0] == '/' && path[1] == 'd' && path[2] == 'i' && path[3] == 's' && path[4] == 'k' && path[5] == '/') {
        const char* rel = path + 6;
        if (rel[0] == 0) return -EINVAL;
        return diskfs_unlink(rel);
    }

    return -ENOSYS;
}

static int syscall_unlinkat_impl(int dirfd, const char* user_path, uint32_t flags) {
    (void)flags;
    if (dirfd != AT_FDCWD) return -ENOSYS;
    return syscall_unlink_impl(user_path);
}

static int syscall_rmdir_impl(const char* user_path) {
    if (!user_path) return -EFAULT;

    char path[128];
    int prc = path_resolve_user(user_path, path, sizeof(path));
    if (prc < 0) return prc;

    if (path[0] == '/' && path[1] == 'd' && path[2] == 'i' && path[3] == 's' && path[4] == 'k' && path[5] == '/') {
        const char* rel = path + 6;
        if (rel[0] == 0) return -EINVAL;
        return diskfs_rmdir(rel);
    }

    return -ENOSYS;
}

static int syscall_rename_impl(const char* user_old, const char* user_new) {
    if (!user_old || !user_new) return -EFAULT;

    char oldp[128];
    char newp[128];
    int rc = path_resolve_user(user_old, oldp, sizeof(oldp));
    if (rc < 0) return rc;
    rc = path_resolve_user(user_new, newp, sizeof(newp));
    if (rc < 0) return rc;

    // Both must be under /disk/
    if (oldp[0] == '/' && oldp[1] == 'd' && oldp[2] == 'i' && oldp[3] == 's' && oldp[4] == 'k' && oldp[5] == '/' &&
        newp[0] == '/' && newp[1] == 'd' && newp[2] == 'i' && newp[3] == 's' && newp[4] == 'k' && newp[5] == '/') {
        const char* old_rel = oldp + 6;
        const char* new_rel = newp + 6;
        if (old_rel[0] == 0 || new_rel[0] == 0) return -EINVAL;
        return diskfs_rename(old_rel, new_rel);
    }

    return -ENOSYS;
}

static int syscall_read_impl(int fd, void* user_buf, uint32_t len) {
    if (len > 1024 * 1024) return -EINVAL;
    if (user_range_ok(user_buf, (size_t)len) == 0) return -EFAULT;

    if (fd == 0 && (!current_process || !current_process->files[0])) {
        return tty_read(user_buf, len);
    }

    if ((fd == 1 || fd == 2) && (!current_process || !current_process->files[fd])) return -EBADF;

    struct file* f = fd_get(fd);
    if (!f || !f->node) return -EBADF;

    int nonblock = (f->flags & O_NONBLOCK) ? 1 : 0;
    if (nonblock) {
        // Non-blocking pipes: if empty but writers exist, return -EAGAIN.
        if (f->node->name[0] == 'p' && f->node->name[1] == 'i' && f->node->name[2] == 'p' && f->node->name[3] == 'e' && f->node->name[4] == ':') {
            struct pipe_node* pn = (struct pipe_node*)f->node;
            struct pipe_state* ps = pn ? pn->ps : 0;
            if (pn && ps && pn->is_read_end && ps->count == 0 && ps->writers != 0) {
                return -EAGAIN;
            }
        }

        // Non-blocking char devices (tty/pty) need special handling, since devfs read blocks.
        if (f->node->flags == FS_CHARDEVICE) {
            if (f->node->inode == 3) {
                if (!tty_can_read()) return -EAGAIN;
            } else if (pty_is_master_ino(f->node->inode)) {
                if (!pty_master_can_read_idx(pty_ino_to_idx(f->node->inode))) return -EAGAIN;
            } else if (pty_is_slave_ino(f->node->inode)) {
                if (!pty_slave_can_read_idx(pty_ino_to_idx(f->node->inode))) return -EAGAIN;
            }
        }
    }

    if (f->node->flags == FS_CHARDEVICE) {
        uint8_t kbuf[256];
        uint32_t total = 0;
        while (total < len) {
            uint32_t chunk = len - total;
            if (chunk > sizeof(kbuf)) chunk = (uint32_t)sizeof(kbuf);

            uint32_t rd = vfs_read(f->node, 0, chunk, kbuf);
            if (rd == 0) break;

            if (copy_to_user((uint8_t*)user_buf + total, kbuf, rd) < 0) {
                return -EFAULT;
            }

            total += rd;
            if (rd < chunk) break;
        }

        return (int)total;
    }

    if (!f->node->read) return -ESPIPE;

    uint8_t kbuf[256];
    uint32_t total = 0;
    while (total < len) {
        uint32_t chunk = len - total;
        if (chunk > sizeof(kbuf)) chunk = (uint32_t)sizeof(kbuf);

        uint32_t rd = vfs_read(f->node, f->offset, chunk, kbuf);
        if (rd == 0) break;

        if (copy_to_user((uint8_t*)user_buf + total, kbuf, rd) < 0) {
            return -EFAULT;
        }

        f->offset += rd;
        total += rd;
        if (rd < chunk) break;
    }

    return (int)total;
}

static int syscall_write_impl(int fd, const void* user_buf, uint32_t len);

static int syscall_write_impl(int fd, const void* user_buf, uint32_t len) {
    if (len > 1024 * 1024) return -EINVAL;
    if (user_range_ok(user_buf, (size_t)len) == 0) return -EFAULT;

    if ((fd == 1 || fd == 2) && (!current_process || !current_process->files[fd])) {
        return tty_write((const char*)user_buf, len);
    }

    if (fd == 0) return -EBADF;

    struct file* f = fd_get(fd);
    if (!f || !f->node) return -EBADF;

    int nonblock = (f->flags & O_NONBLOCK) ? 1 : 0;
    if (nonblock) {
        // Non-blocking pipe write: if full but readers exist, return -EAGAIN.
        if (f->node->name[0] == 'p' && f->node->name[1] == 'i' && f->node->name[2] == 'p' && f->node->name[3] == 'e' && f->node->name[4] == ':') {
            struct pipe_node* pn = (struct pipe_node*)f->node;
            struct pipe_state* ps = pn ? pn->ps : 0;
            if (pn && ps && !pn->is_read_end) {
                if (ps->readers != 0 && (ps->cap - ps->count) == 0) {
                    return -EAGAIN;
                }
            }
        }
    }
    if (!f->node->write) return -ESPIPE;
    if (((f->node->flags & FS_FILE) == 0) && f->node->flags != FS_CHARDEVICE) return -ESPIPE;

    uint8_t kbuf[256];
    uint32_t total = 0;
    while (total < len) {
        uint32_t chunk = len - total;
        if (chunk > sizeof(kbuf)) chunk = (uint32_t)sizeof(kbuf);

        if (copy_from_user(kbuf, (const uint8_t*)user_buf + total, chunk) < 0) {
            return -EFAULT;
        }

        uint32_t wr = vfs_write(f->node, ((f->node->flags & FS_FILE) != 0) ? f->offset : 0, chunk, kbuf);
        if (wr == 0) break;
        if ((f->node->flags & FS_FILE) != 0) f->offset += wr;
        total += wr;
        if (wr < chunk) break;
    }
    return (int)total;
}

static int syscall_ioctl_impl(int fd, uint32_t cmd, void* user_arg) {
    struct file* f = fd_get(fd);
    if (!f || !f->node) return -EBADF;

    fs_node_t* n = f->node;
    if (n->flags != FS_CHARDEVICE) return -ENOTTY;
    if (n->inode == 3) return tty_ioctl(cmd, user_arg);
    if (pty_is_slave_ino(n->inode)) return pty_slave_ioctl_idx(pty_ino_to_idx(n->inode), cmd, user_arg);
    if (pty_is_master_ino(n->inode)) return -ENOTTY;
    return -ENOTTY;
}

static int syscall_setsid_impl(void) {
    if (!current_process) return -EINVAL;
    if (current_process->pid != 0 && current_process->pgrp_id == current_process->pid) return -EPERM;
    current_process->session_id = current_process->pid;
    current_process->pgrp_id = current_process->pid;
    return (int)current_process->session_id;
}

static int syscall_setpgid_impl(int pid, int pgid) {
    if (!current_process) return -EINVAL;
    if (pid != 0 && pid != (int)current_process->pid) return -EINVAL;
    if (pgid == 0) pgid = (int)current_process->pid;
    if (pgid < 0) return -EINVAL;
    current_process->pgrp_id = (uint32_t)pgid;
    return 0;
}

static int syscall_getpgrp_impl(void) {
    if (!current_process) return 0;
    return (int)current_process->pgrp_id;
}

static int syscall_sigaction_impl(int sig, const struct sigaction* user_act, struct sigaction* user_oldact) {
    if (!current_process) return -EINVAL;
    if (sig <= 0 || sig >= PROCESS_MAX_SIG) return -EINVAL;

    if (user_oldact) {
        if (user_range_ok(user_oldact, sizeof(*user_oldact)) == 0) return -EFAULT;
        struct sigaction old = current_process->sigactions[sig];
        if (copy_to_user(user_oldact, &old, sizeof(old)) < 0) return -EFAULT;
    }

    if (!user_act) {
        return 0;
    }

    if (user_range_ok(user_act, sizeof(*user_act)) == 0) return -EFAULT;
    struct sigaction act;
    if (copy_from_user(&act, user_act, sizeof(act)) < 0) return -EFAULT;
    current_process->sigactions[sig] = act;
    return 0;
}

static int syscall_sigprocmask_impl(uint32_t how, uint32_t mask, uint32_t* old_out) {
    if (!current_process) return -EINVAL;

    if (old_out) {
        if (user_range_ok(old_out, sizeof(*old_out)) == 0) return -EFAULT;
        uint32_t old = current_process->sig_blocked_mask;
        if (copy_to_user(old_out, &old, sizeof(old)) < 0) return -EFAULT;
    }

    if (how == 0U) {
        current_process->sig_blocked_mask = mask;
        return 0;
    }
    if (how == 1U) {
        current_process->sig_blocked_mask |= mask;
        return 0;
    }
    if (how == 2U) {
        current_process->sig_blocked_mask &= ~mask;
        return 0;
    }
    return -EINVAL;
}

static int syscall_sigreturn_impl(struct registers* regs, const struct sigframe* user_frame) {
    if (!regs) return -EINVAL;
    if (!current_process) return -EINVAL;
    if ((regs->cs & 3U) != 3U) return -EPERM;
    if (!user_frame) return -EFAULT;

    if (user_range_ok(user_frame, sizeof(*user_frame)) == 0) { return -EFAULT; }

    struct sigframe f;
    if (copy_from_user(&f, user_frame, sizeof(f)) < 0) return -EFAULT;
    if (f.magic != SIGFRAME_MAGIC) { return -EINVAL; }

    if ((f.saved.cs & 3U) != 3U) return -EPERM;
    if ((f.saved.ss & 3U) != 3U) return -EPERM;

    // Sanitize eflags: clear IOPL (bits 12-13) to prevent privilege escalation,
    // ensure IF (bit 9) is set so interrupts remain enabled in usermode.
    f.saved.eflags = (f.saved.eflags & ~0x3000U) | 0x200U;

    // Restore the full saved trapframe. The interrupt stub will pop these regs and iret.
    *regs = f.saved;
    return 0;
}

struct timespec {
    uint32_t tv_sec;
    uint32_t tv_nsec;
};

enum {
    CLOCK_REALTIME = 0,
    CLOCK_MONOTONIC = 1,
};

static int syscall_nanosleep_impl(const struct timespec* user_req, struct timespec* user_rem) {
    if (!user_req) return -EFAULT;
    if (user_range_ok(user_req, sizeof(struct timespec)) == 0) return -EFAULT;

    struct timespec req;
    if (copy_from_user(&req, user_req, sizeof(req)) < 0) return -EFAULT;

    if (req.tv_nsec >= 1000000000U) return -EINVAL;

    const uint32_t TICK_MS = 20;
    uint32_t ms = req.tv_sec * 1000U + req.tv_nsec / 1000000U;
    uint32_t ticks = (ms + TICK_MS - 1) / TICK_MS;
    if (ticks == 0 && (req.tv_sec > 0 || req.tv_nsec > 0)) ticks = 1;

    if (ticks > 0) {
        process_sleep(ticks);
    }

    if (user_rem) {
        if (user_range_ok(user_rem, sizeof(struct timespec)) != 0) {
            struct timespec rem = {0, 0};
            (void)copy_to_user(user_rem, &rem, sizeof(rem));
        }
    }

    return 0;
}

static int syscall_clock_gettime_impl(uint32_t clk_id, struct timespec* user_tp) {
    if (!user_tp) return -EFAULT;
    if (user_range_ok(user_tp, sizeof(struct timespec)) == 0) return -EFAULT;

    if (clk_id != CLOCK_REALTIME && clk_id != CLOCK_MONOTONIC) return -EINVAL;

    uint32_t ticks = get_tick_count();
    const uint32_t TICK_MS = 20;
    uint32_t total_ms = ticks * TICK_MS;

    struct timespec tp;
    tp.tv_sec = total_ms / 1000U;
    tp.tv_nsec = (total_ms % 1000U) * 1000000U;

    if (copy_to_user(user_tp, &tp, sizeof(tp)) < 0) return -EFAULT;
    return 0;
}

enum {
    PROT_NONE  = 0x0,
    PROT_READ  = 0x1,
    PROT_WRITE = 0x2,
    PROT_EXEC  = 0x4,
};

enum {
    MAP_SHARED    = 0x01,
    MAP_PRIVATE   = 0x02,
    MAP_FIXED     = 0x10,
    MAP_ANONYMOUS = 0x20,
};

static uintptr_t mmap_find_free(uint32_t length) {
    if (!current_process) return 0;
    const uintptr_t MMAP_BASE = 0x40000000U;
    const uintptr_t MMAP_END  = 0x7FF00000U;

    for (uintptr_t candidate = MMAP_BASE; candidate + length <= MMAP_END; candidate += 0x1000U) {
        int overlap = 0;
        for (int i = 0; i < PROCESS_MAX_MMAPS; i++) {
            if (current_process->mmaps[i].length == 0) continue;
            uintptr_t mb = current_process->mmaps[i].base;
            uint32_t ml = current_process->mmaps[i].length;
            if (candidate < mb + ml && candidate + length > mb) {
                overlap = 1;
                candidate = ((mb + ml + 0xFFFU) & ~(uintptr_t)0xFFFU) - 0x1000U;
                break;
            }
        }
        if (!overlap) return candidate;
    }
    return 0;
}

static uintptr_t syscall_mmap_impl(uintptr_t addr, uint32_t length, uint32_t prot,
                                    uint32_t flags, int fd, uint32_t offset) {
    (void)offset;
    if (!current_process) return (uintptr_t)-EINVAL;
    if (length == 0) return (uintptr_t)-EINVAL;

    if (!(flags & MAP_ANONYMOUS)) return (uintptr_t)-ENOSYS;
    if (fd != -1) return (uintptr_t)-EINVAL;

    uint32_t aligned_len = (length + 0xFFFU) & ~(uint32_t)0xFFFU;

    uintptr_t base;
    if (flags & MAP_FIXED) {
        if (addr == 0 || (addr & 0xFFF)) return (uintptr_t)-EINVAL;
        if (hal_mm_kernel_virt_base() && addr >= hal_mm_kernel_virt_base()) return (uintptr_t)-EINVAL;
        base = addr;
    } else {
        base = mmap_find_free(aligned_len);
        if (!base) return (uintptr_t)-ENOMEM;
    }

    int slot = -1;
    for (int i = 0; i < PROCESS_MAX_MMAPS; i++) {
        if (current_process->mmaps[i].length == 0) { slot = i; break; }
    }
    if (slot < 0) return (uintptr_t)-ENOMEM;

    uint32_t vmm_flags = VMM_FLAG_PRESENT | VMM_FLAG_USER;
    if (prot & PROT_WRITE) vmm_flags |= VMM_FLAG_RW;

    for (uintptr_t va = base; va < base + aligned_len; va += 0x1000U) {
        void* frame = pmm_alloc_page();
        if (!frame) return (uintptr_t)-ENOMEM;
        vmm_map_page((uint64_t)(uintptr_t)frame, (uint64_t)va, vmm_flags);
        memset((void*)va, 0, 0x1000U);
    }

    current_process->mmaps[slot].base = base;
    current_process->mmaps[slot].length = aligned_len;
    current_process->mmaps[slot].shmid = -1;

    return base;
}

static int syscall_munmap_impl(uintptr_t addr, uint32_t length) {
    if (!current_process) return -EINVAL;
    if (addr == 0 || (addr & 0xFFF)) return -EINVAL;
    if (length == 0) return -EINVAL;

    uint32_t aligned_len = (length + 0xFFFU) & ~(uint32_t)0xFFFU;

    int found = -1;
    for (int i = 0; i < PROCESS_MAX_MMAPS; i++) {
        if (current_process->mmaps[i].base == addr &&
            current_process->mmaps[i].length == aligned_len) {
            found = i;
            break;
        }
    }
    if (found < 0) return -EINVAL;

    for (uintptr_t va = addr; va < addr + aligned_len; va += 0x1000U) {
        vmm_unmap_page((uint64_t)va);
    }

    current_process->mmaps[found].base = 0;
    current_process->mmaps[found].length = 0;
    current_process->mmaps[found].shmid = -1;
    return 0;
}

static uintptr_t syscall_brk_impl(uintptr_t addr) {
    if (!current_process) return 0;

    if (addr == 0) {
        return current_process->heap_break;
    }

    const uintptr_t KERN_BASE = hal_mm_kernel_virt_base();
    const uintptr_t USER_STACK_BASE = 0x00800000U;

    if (addr < current_process->heap_start) return current_process->heap_break;
    if (addr >= USER_STACK_BASE) return current_process->heap_break;
    if (KERN_BASE && addr >= KERN_BASE) return current_process->heap_break;

    uintptr_t old_brk = current_process->heap_break;
    uintptr_t new_brk = (addr + 0xFFFU) & ~(uintptr_t)0xFFFU;
    uintptr_t old_brk_page = (old_brk + 0xFFFU) & ~(uintptr_t)0xFFFU;

    if (new_brk > old_brk_page) {
        for (uintptr_t va = old_brk_page; va < new_brk; va += 0x1000U) {
            void* frame = pmm_alloc_page();
            if (!frame) {
                return current_process->heap_break;
            }
            vmm_as_map_page(current_process->addr_space,
                            (uint64_t)(uintptr_t)frame, (uint64_t)va,
                            VMM_FLAG_PRESENT | VMM_FLAG_RW | VMM_FLAG_USER);
            memset((void*)va, 0, 0x1000U);
        }
    } else if (new_brk < old_brk_page) {
        for (uintptr_t va = new_brk; va < old_brk_page; va += 0x1000U) {
            vmm_unmap_page((uint64_t)va);
        }
    }

    current_process->heap_break = addr;
    return addr;
}

void syscall_handler(struct registers* regs) {
    uint32_t syscall_no = regs->eax;

    if (syscall_no == SYSCALL_WRITE) {
        uint32_t fd = regs->ebx;
        const char* buf = (const char*)regs->ecx;
        uint32_t len = regs->edx;

        regs->eax = (uint32_t)syscall_write_impl((int)fd, buf, len);
        return;
    }

    if (syscall_no == SYSCALL_GETPID) {
        regs->eax = current_process ? current_process->pid : 0;
        return;
    }

    if (syscall_no == SYSCALL_GETPPID) {
        regs->eax = current_process ? current_process->parent_pid : 0;
        return;
    }

    if (syscall_no == SYSCALL_OPEN) {
        const char* path = (const char*)regs->ebx;
        uint32_t flags = regs->ecx;
        regs->eax = (uint32_t)syscall_open_impl(path, flags);
        return;
    }

    if (syscall_no == SYSCALL_OPENAT) {
        int dirfd = (int)regs->ebx;
        const char* path = (const char*)regs->ecx;
        uint32_t flags = (uint32_t)regs->edx;
        uint32_t mode = (uint32_t)regs->esi;
        regs->eax = (uint32_t)syscall_openat_impl(dirfd, path, flags, mode);
        return;
    }

    if (syscall_no == SYSCALL_CHDIR) {
        const char* path = (const char*)regs->ebx;
        regs->eax = (uint32_t)syscall_chdir_impl(path);
        return;
    }

    if (syscall_no == SYSCALL_GETCWD) {
        char* buf = (char*)regs->ebx;
        uint32_t size = (uint32_t)regs->ecx;
        regs->eax = (uint32_t)syscall_getcwd_impl(buf, size);
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

        for (int fd = 0; fd < PROCESS_MAX_FILES; fd++) {
            if (current_process && current_process->files[fd]) {
                (void)fd_close(fd);
            }
        }

        process_exit_notify(status);

        hal_cpu_enable_interrupts();
        schedule();
        for(;;) {
            hal_cpu_disable_interrupts();
            hal_cpu_idle();
        }
    }

    if (syscall_no == SYSCALL_WAITPID) {
        int pid = (int)regs->ebx;
        int* user_status = (int*)regs->ecx;
        uint32_t options = regs->edx;

        if (user_status && user_range_ok(user_status, sizeof(int)) == 0) {
            regs->eax = (uint32_t)-EFAULT;
            return;
        }

        int status = 0;
        int retpid = process_waitpid(pid, &status, options);
        if (retpid < 0) {
            regs->eax = (uint32_t)retpid;
            return;
        }

        if (retpid == 0) {
            regs->eax = 0;
            return;
        }

        if (user_status) {
            if (copy_to_user(user_status, &status, sizeof(status)) < 0) {
                regs->eax = (uint32_t)-EFAULT;
                return;
            }
        }

        regs->eax = (uint32_t)retpid;
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
        struct stat* user_st = (struct stat*)regs->ecx;
        regs->eax = (uint32_t)syscall_stat_impl(path, user_st);
        return;
    }

    if (syscall_no == SYSCALL_FSTATAT) {
        int dirfd = (int)regs->ebx;
        const char* path = (const char*)regs->ecx;
        struct stat* user_st = (struct stat*)regs->edx;
        uint32_t flags = (uint32_t)regs->esi;
        regs->eax = (uint32_t)syscall_fstatat_impl(dirfd, path, user_st, flags);
        return;
    }

    if (syscall_no == SYSCALL_DUP) {
        int oldfd = (int)regs->ebx;
        regs->eax = (uint32_t)syscall_dup_impl(oldfd);
        return;
    }

    if (syscall_no == SYSCALL_DUP2) {
        int oldfd = (int)regs->ebx;
        int newfd = (int)regs->ecx;
        regs->eax = (uint32_t)syscall_dup2_impl(oldfd, newfd);
        return;
    }

    if (syscall_no == SYSCALL_DUP3) {
        int oldfd = (int)regs->ebx;
        int newfd = (int)regs->ecx;
        uint32_t flags = (uint32_t)regs->edx;
        regs->eax = (uint32_t)syscall_dup3_impl(oldfd, newfd, flags);
        return;
    }

    if (syscall_no == SYSCALL_PIPE) {
        int* user_fds = (int*)regs->ebx;
        regs->eax = (uint32_t)syscall_pipe_impl(user_fds);
        return;
    }

    if (syscall_no == SYSCALL_PIPE2) {
        int* user_fds = (int*)regs->ebx;
        uint32_t flags = (uint32_t)regs->ecx;
        regs->eax = (uint32_t)syscall_pipe2_impl(user_fds, flags);
        return;
    }

    if (syscall_no == SYSCALL_EXECVE) {
        const char* path = (const char*)regs->ebx;
        const char* const* argv = (const char* const*)regs->ecx;
        const char* const* envp = (const char* const*)regs->edx;
        regs->eax = (uint32_t)syscall_execve_impl(regs, path, argv, envp);
        return;
    }

    if (syscall_no == SYSCALL_FORK) {
        regs->eax = (uint32_t)syscall_fork_impl(regs);
        return;
    }

    if (syscall_no == SYSCALL_POLL) {
        struct pollfd* fds = (struct pollfd*)regs->ebx;
        uint32_t nfds = regs->ecx;
        int32_t timeout = (int32_t)regs->edx;
        regs->eax = (uint32_t)syscall_poll_impl(fds, nfds, timeout);
        return;
    }

    if (syscall_no == SYSCALL_KILL) {
        uint32_t pid = regs->ebx;
        int sig = (int)regs->ecx;
        regs->eax = (uint32_t)process_kill(pid, sig);
        return;
    }

    if (syscall_no == SYSCALL_SELECT) {
        uint32_t nfds = regs->ebx;
        uint64_t* readfds = (uint64_t*)regs->ecx;
        uint64_t* writefds = (uint64_t*)regs->edx;
        uint64_t* exceptfds = (uint64_t*)regs->esi;
        int32_t timeout = (int32_t)regs->edi;
        regs->eax = (uint32_t)syscall_select_impl(nfds, readfds, writefds, exceptfds, timeout);
        return;
    }

    if (syscall_no == SYSCALL_IOCTL) {
        int fd = (int)regs->ebx;
        uint32_t cmd = (uint32_t)regs->ecx;
        void* arg = (void*)regs->edx;
        regs->eax = (uint32_t)syscall_ioctl_impl(fd, cmd, arg);
        return;
    }

    if (syscall_no == SYSCALL_SETSID) {
        regs->eax = (uint32_t)syscall_setsid_impl();
        return;
    }

    if (syscall_no == SYSCALL_SETPGID) {
        int pid = (int)regs->ebx;
        int pgid = (int)regs->ecx;
        regs->eax = (uint32_t)syscall_setpgid_impl(pid, pgid);
        return;
    }

    if (syscall_no == SYSCALL_GETPGRP) {
        regs->eax = (uint32_t)syscall_getpgrp_impl();
        return;
    }

    if (syscall_no == SYSCALL_SIGACTION) {
        int sig = (int)regs->ebx;
        const struct sigaction* act = (const struct sigaction*)regs->ecx;
        struct sigaction* oldact = (struct sigaction*)regs->edx;
        regs->eax = (uint32_t)syscall_sigaction_impl(sig, act, oldact);
        return;
    }

    if (syscall_no == SYSCALL_SIGPROCMASK) {
        uint32_t how = regs->ebx;
        uint32_t mask = regs->ecx;
        uint32_t* old_out = (uint32_t*)regs->edx;
        regs->eax = (uint32_t)syscall_sigprocmask_impl(how, mask, old_out);
        return;
    }

    if (syscall_no == SYSCALL_SIGRETURN) {
        const struct sigframe* user_frame = (const struct sigframe*)regs->ebx;
        regs->eax = (uint32_t)syscall_sigreturn_impl(regs, user_frame);
        return;
    }

    if (syscall_no == SYSCALL_FCNTL) {
        int fd = (int)regs->ebx;
        int cmd = (int)regs->ecx;
        uint32_t arg = (uint32_t)regs->edx;
        regs->eax = (uint32_t)syscall_fcntl_impl(fd, cmd, arg);
        return;
    }

    if (syscall_no == SYSCALL_MKDIR) {
        const char* path = (const char*)regs->ebx;
        regs->eax = (uint32_t)syscall_mkdir_impl(path);
        return;
    }

    if (syscall_no == SYSCALL_UNLINK) {
        const char* path = (const char*)regs->ebx;
        regs->eax = (uint32_t)syscall_unlink_impl(path);
        return;
    }

    if (syscall_no == SYSCALL_UNLINKAT) {
        int dirfd = (int)regs->ebx;
        const char* path = (const char*)regs->ecx;
        uint32_t flags = (uint32_t)regs->edx;
        regs->eax = (uint32_t)syscall_unlinkat_impl(dirfd, path, flags);
        return;
    }

    if (syscall_no == SYSCALL_GETDENTS) {
        int fd = (int)regs->ebx;
        void* buf = (void*)regs->ecx;
        uint32_t len = (uint32_t)regs->edx;
        regs->eax = (uint32_t)syscall_getdents_impl(fd, buf, len);
        return;
    }

    if (syscall_no == SYSCALL_RENAME) {
        const char* oldpath = (const char*)regs->ebx;
        const char* newpath = (const char*)regs->ecx;
        regs->eax = (uint32_t)syscall_rename_impl(oldpath, newpath);
        return;
    }

    if (syscall_no == SYSCALL_RMDIR) {
        const char* path = (const char*)regs->ebx;
        regs->eax = (uint32_t)syscall_rmdir_impl(path);
        return;
    }

    if (syscall_no == SYSCALL_BRK) {
        uintptr_t addr = (uintptr_t)regs->ebx;
        regs->eax = (uint32_t)syscall_brk_impl(addr);
        return;
    }

    if (syscall_no == SYSCALL_NANOSLEEP) {
        const struct timespec* req = (const struct timespec*)regs->ebx;
        struct timespec* rem = (struct timespec*)regs->ecx;
        regs->eax = (uint32_t)syscall_nanosleep_impl(req, rem);
        return;
    }

    if (syscall_no == SYSCALL_CLOCK_GETTIME) {
        uint32_t clk_id = regs->ebx;
        struct timespec* tp = (struct timespec*)regs->ecx;
        regs->eax = (uint32_t)syscall_clock_gettime_impl(clk_id, tp);
        return;
    }

    if (syscall_no == SYSCALL_MMAP) {
        uintptr_t addr = (uintptr_t)regs->ebx;
        uint32_t length = regs->ecx;
        uint32_t prot = regs->edx;
        uint32_t mflags = regs->esi;
        int fd = (int)regs->edi;
        regs->eax = (uint32_t)syscall_mmap_impl(addr, length, prot, mflags, fd, 0);
        return;
    }

    if (syscall_no == SYSCALL_MUNMAP) {
        uintptr_t addr = (uintptr_t)regs->ebx;
        uint32_t length = regs->ecx;
        regs->eax = (uint32_t)syscall_munmap_impl(addr, length);
        return;
    }

    if (syscall_no == SYSCALL_SHMGET) {
        uint32_t key = regs->ebx;
        uint32_t size = regs->ecx;
        int flags = (int)regs->edx;
        regs->eax = (uint32_t)shm_get(key, size, flags);
        return;
    }

    if (syscall_no == SYSCALL_SHMAT) {
        int shmid = (int)regs->ebx;
        uintptr_t shmaddr = (uintptr_t)regs->ecx;
        regs->eax = (uint32_t)(uintptr_t)shm_at(shmid, shmaddr);
        return;
    }

    if (syscall_no == SYSCALL_SHMDT) {
        const void* shmaddr = (const void*)regs->ebx;
        regs->eax = (uint32_t)shm_dt(shmaddr);
        return;
    }

    if (syscall_no == SYSCALL_SHMCTL) {
        int shmid = (int)regs->ebx;
        int cmd = (int)regs->ecx;
        struct shmid_ds* buf = (struct shmid_ds*)regs->edx;
        regs->eax = (uint32_t)shm_ctl(shmid, cmd, buf);
        return;
    }

    regs->eax = (uint32_t)-ENOSYS;
}

void syscall_init(void) {
#if defined(__i386__)
    register_interrupt_handler(128, syscall_handler);
    x86_sysenter_init();
#endif
}
