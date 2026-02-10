#include "syscall.h"
#include "idt.h"
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
#include "elf.h"
#include "stat.h"
#include "vmm.h"

#include "hal/cpu.h"

#include <stddef.h>

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

    uintptr_t child_as = vmm_as_clone_user(src_as);
    if (!child_as) return -ENOMEM;

    struct registers child_regs = *regs;
    child_regs.eax = 0;

    struct process* child = process_fork_create(child_as, &child_regs);
    if (!child) {
        vmm_as_destroy(child_as);
        return -ENOMEM;
    }

    for (int fd = 0; fd < PROCESS_MAX_FILES; fd++) {
        struct file* f = current_process->files[fd];
        if (!f) continue;
        f->refcount++;
        child->files[fd] = f;
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
                } else if (n->inode == 4) {
                    if ((kfds[i].events & POLLIN) && pty_master_can_read()) kfds[i].revents |= POLLIN;
                    if ((kfds[i].events & POLLOUT) && pty_master_can_write()) kfds[i].revents |= POLLOUT;
                } else if (n->inode == 6) {
                    if ((kfds[i].events & POLLIN) && pty_slave_can_read()) kfds[i].revents |= POLLIN;
                    if ((kfds[i].events & POLLOUT) && pty_slave_can_write()) kfds[i].revents |= POLLOUT;
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

static int syscall_pipe_impl(int* user_fds) {
    if (!user_fds) return -EFAULT;
    if (user_range_ok(user_fds, sizeof(int) * 2) == 0) return -EFAULT;

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
    if (pipe_node_create(ps, 1, &rnode) < 0 || pipe_node_create(ps, 0, &wnode) < 0) {
        if (rnode) vfs_close(rnode);
        if (wnode) vfs_close(wnode);
        if (ps->buf) kfree(ps->buf);
        kfree(ps);
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

    int kfds[2];
    kfds[0] = rfd;
    kfds[1] = wfd;
    if (copy_to_user(user_fds, kfds, sizeof(kfds)) < 0) {
        (void)fd_close(rfd);
        (void)fd_close(wfd);
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

    if (f->refcount > 0) {
        f->refcount--;
    }
    if (f->refcount == 0) {
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
    f->refcount++;
    int newfd = fd_alloc_from(0, f);
    if (newfd < 0) {
        f->refcount--;
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
    if (elf32_load_user_from_initrd(path, &entry, &user_sp, &new_as) != 0) {
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
    vmm_as_activate(new_as);

    // Build a minimal initial user stack: argc, argv pointers, envp pointers, strings.
    // The loader returns a fresh stack top (user_sp). We'll pack strings below it.
    uintptr_t sp = user_sp;
    sp &= ~(uintptr_t)0xF;

    uintptr_t argv_ptrs_va[EXECVE_MAX_ARGC + 1];
    uintptr_t envp_ptrs_va[EXECVE_MAX_ENVC + 1];

    for (int i = envc - 1; i >= 0; i--) {
        size_t len = strlen(kenvp[i]) + 1;
        sp -= len;
        memcpy((void*)sp, kenvp[i], len);
        envp_ptrs_va[i] = sp;
    }
    envp_ptrs_va[envc] = 0;

    for (int i = argc - 1; i >= 0; i--) {
        size_t len = strlen(kargv[i]) + 1;
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

    f->refcount++;
    current_process->files[newfd] = f;
    return newfd;
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
    if (!user_path) return -EFAULT;

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
    f->flags = 0;
    f->refcount = 1;

    int fd = fd_alloc(f);
    if (fd < 0) {
        kfree(f);
        return -EMFILE;
    }
    return fd;
}

static int syscall_mkdir_impl(const char* user_path) {
    if (!user_path) return -EFAULT;

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

    // For now only support diskfs dirs (mounted at /disk). We encode diskfs inode as 100+ino.
    if (f->node->inode < 100U) return -ENOSYS;
    uint16_t dir_ino = (uint16_t)(f->node->inode - 100U);

    uint8_t kbuf[256];
    if (len < (uint32_t)sizeof(kbuf)) {
        // keep behavior simple: require small buffers too
    }
    uint32_t klen = len;
    if (klen > (uint32_t)sizeof(kbuf)) klen = (uint32_t)sizeof(kbuf);

    uint32_t idx = f->offset;
    int rc = diskfs_getdents(dir_ino, &idx, kbuf, klen);
    if (rc < 0) return rc;
    if (rc == 0) return 0;

    if (copy_to_user(user_buf, kbuf, (uint32_t)rc) < 0) return -EFAULT;
    f->offset = idx;
    return rc;
}

static int syscall_unlink_impl(const char* user_path) {
    if (!user_path) return -EFAULT;

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

    if (path[0] == '/' && path[1] == 'd' && path[2] == 'i' && path[3] == 's' && path[4] == 'k' && path[5] == '/') {
        const char* rel = path + 6;
        if (rel[0] == 0) return -EINVAL;
        return diskfs_unlink(rel);
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

static int syscall_write_impl(int fd, const void* user_buf, uint32_t len) {
    if (len > 1024 * 1024) return -EINVAL;
    if (user_range_ok(user_buf, (size_t)len) == 0) return -EFAULT;

    if ((fd == 1 || fd == 2) && (!current_process || !current_process->files[fd])) {
        return tty_write((const char*)user_buf, len);
    }

    if (fd == 0) return -EBADF;

    struct file* f = fd_get(fd);
    if (!f || !f->node) return -EBADF;
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
    if (n->inode == 6) return pty_slave_ioctl(cmd, user_arg);
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

    // Restore the full saved trapframe. The interrupt stub will pop these regs and iret.
    *regs = f.saved;
    return 0;
}

static void syscall_handler(struct registers* regs) {
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
            __asm__ volatile("cli; hlt");
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
        struct stat* st = (struct stat*)regs->ecx;
        regs->eax = (uint32_t)syscall_stat_impl(path, st);
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

    if (syscall_no == SYSCALL_PIPE) {
        int* user_fds = (int*)regs->ebx;
        regs->eax = (uint32_t)syscall_pipe_impl(user_fds);
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

    if (syscall_no == SYSCALL_GETDENTS) {
        int fd = (int)regs->ebx;
        void* buf = (void*)regs->ecx;
        uint32_t len = (uint32_t)regs->edx;
        regs->eax = (uint32_t)syscall_getdents_impl(fd, buf, len);
        return;
    }

    regs->eax = (uint32_t)-ENOSYS;
}

void syscall_init(void) {
#if defined(__i386__)
    register_interrupt_handler(128, syscall_handler);
#endif
}
