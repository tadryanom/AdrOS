#include "syscall.h"
#include "interrupts.h"
#include "fs.h"
#include "process.h"
#include "spinlock.h"
#include "uaccess.h"
#include "console.h"
#include "utils.h"

#include "heap.h"
#include "tty.h"
#include "pty.h"
#include "tmpfs.h"

#include "errno.h"
#include "shm.h"
#include "socket.h"

#include "elf.h"
#include "stat.h"
#include "timer.h"
#include "vmm.h"
#include "pmm.h"
#include "hal/mm.h"

#include "hal/cpu.h"
#include "arch_signal.h"
#include "arch_syscall.h"
#include "arch_process.h"
#include "rtc.h"

#include <stddef.h>

enum {
    O_APPEND   = 0x400,
    O_NONBLOCK = 0x800,
    O_CLOEXEC  = 0x80000,
};

/* Kernel-side itimerval for setitimer/getitimer (matches userland layout) */
struct k_timeval {
    uint32_t tv_sec;
    uint32_t tv_usec;
};
struct k_itimerval {
    struct k_timeval it_interval;
    struct k_timeval it_value;
};
#define ITIMER_REAL    0
#define ITIMER_VIRTUAL 1
#define ITIMER_PROF    2
#define TICKS_PER_SEC  TIMER_HZ
#define USEC_PER_TICK  (1000000U / TICKS_PER_SEC)

static uint32_t timeval_to_ticks(const struct k_timeval* tv) {
    return tv->tv_sec * TICKS_PER_SEC + tv->tv_usec / USEC_PER_TICK;
}
static void ticks_to_timeval(uint32_t ticks, struct k_timeval* tv) {
    tv->tv_sec  = ticks / TICKS_PER_SEC;
    tv->tv_usec = (ticks % TICKS_PER_SEC) * USEC_PER_TICK;
}

enum {
    FD_CLOEXEC = 1,
};

/* --- POSIX message queues --- */
#define MQ_MAX_QUEUES  8
#define MQ_MAX_MSGS    16
#define MQ_MSG_SIZE    256

struct mq_msg {
    uint8_t  data[MQ_MSG_SIZE];
    uint32_t len;
    uint32_t prio;
};

struct mq_queue {
    int      active;
    char     name[32];
    struct mq_msg msgs[MQ_MAX_MSGS];
    uint32_t head;
    uint32_t tail;
    uint32_t count;
    uint32_t maxmsg;
    uint32_t msgsize;
};

static struct mq_queue mq_table[MQ_MAX_QUEUES];
static spinlock_t mq_lock = {0};

static int mq_find_by_name(const char* name) {
    for (int i = 0; i < MQ_MAX_QUEUES; i++) {
        if (mq_table[i].active && strcmp(mq_table[i].name, name) == 0)
            return i;
    }
    return -1;
}

static int syscall_mq_open_impl(const char* user_name, uint32_t oflag) {
    char name[32];
    if (copy_from_user(name, user_name, 31) < 0) return -EFAULT;
    name[31] = 0;

    uintptr_t fl = spin_lock_irqsave(&mq_lock);
    int idx = mq_find_by_name(name);
    if (idx >= 0) {
        spin_unlock_irqrestore(&mq_lock, fl);
        return idx;
    }
    if (!(oflag & 0x40U)) { /* O_CREAT */
        spin_unlock_irqrestore(&mq_lock, fl);
        return -ENOENT;
    }
    for (int i = 0; i < MQ_MAX_QUEUES; i++) {
        if (!mq_table[i].active) {
            memset(&mq_table[i], 0, sizeof(mq_table[i]));
            mq_table[i].active = 1;
            strcpy(mq_table[i].name, name);
            mq_table[i].maxmsg = MQ_MAX_MSGS;
            mq_table[i].msgsize = MQ_MSG_SIZE;
            spin_unlock_irqrestore(&mq_lock, fl);
            return i;
        }
    }
    spin_unlock_irqrestore(&mq_lock, fl);
    return -ENOSPC;
}

static int syscall_mq_close_impl(int mqd) {
    (void)mqd;
    return 0;
}

static int syscall_mq_send_impl(int mqd, const void* user_buf, uint32_t len, uint32_t prio) {
    if (mqd < 0 || mqd >= MQ_MAX_QUEUES) return -EBADF;
    if (len > MQ_MSG_SIZE) return -EMSGSIZE;

    uintptr_t fl = spin_lock_irqsave(&mq_lock);
    struct mq_queue* q = &mq_table[mqd];
    if (!q->active) { spin_unlock_irqrestore(&mq_lock, fl); return -EBADF; }
    if (q->count >= q->maxmsg) { spin_unlock_irqrestore(&mq_lock, fl); return -EAGAIN; }

    struct mq_msg* m = &q->msgs[q->tail];
    spin_unlock_irqrestore(&mq_lock, fl);

    if (copy_from_user(m->data, user_buf, len) < 0) return -EFAULT;
    m->len = len;
    m->prio = prio;

    fl = spin_lock_irqsave(&mq_lock);
    q->tail = (q->tail + 1) % q->maxmsg;
    q->count++;
    spin_unlock_irqrestore(&mq_lock, fl);
    return 0;
}

static int syscall_mq_receive_impl(int mqd, void* user_buf, uint32_t len, uint32_t* user_prio) {
    if (mqd < 0 || mqd >= MQ_MAX_QUEUES) return -EBADF;

    uintptr_t fl = spin_lock_irqsave(&mq_lock);
    struct mq_queue* q = &mq_table[mqd];
    if (!q->active) { spin_unlock_irqrestore(&mq_lock, fl); return -EBADF; }
    if (q->count == 0) { spin_unlock_irqrestore(&mq_lock, fl); return -EAGAIN; }

    struct mq_msg* m = &q->msgs[q->head];
    uint32_t mlen = m->len;
    uint32_t mprio = m->prio;
    if (mlen > len) mlen = len;

    q->head = (q->head + 1) % q->maxmsg;
    q->count--;
    spin_unlock_irqrestore(&mq_lock, fl);

    if (copy_to_user(user_buf, m->data, mlen) < 0) return -EFAULT;
    if (user_prio) {
        if (user_range_ok(user_prio, 4))
            (void)copy_to_user(user_prio, &mprio, 4);
    }
    return (int)mlen;
}

static int syscall_mq_unlink_impl(const char* user_name) {
    char name[32];
    if (copy_from_user(name, user_name, 31) < 0) return -EFAULT;
    name[31] = 0;

    uintptr_t fl = spin_lock_irqsave(&mq_lock);
    int idx = mq_find_by_name(name);
    if (idx < 0) { spin_unlock_irqrestore(&mq_lock, fl); return -ENOENT; }
    mq_table[idx].active = 0;
    spin_unlock_irqrestore(&mq_lock, fl);
    return 0;
}

/* --- POSIX named semaphores --- */
#define SEM_MAX  16

struct ksem_named {
    int      active;
    char     name[32];
    int32_t  value;
    spinlock_t lock;
};

static struct ksem_named sem_table[SEM_MAX];
static spinlock_t sem_table_lock = {0};

static int syscall_sem_open_impl(const char* user_name, uint32_t oflag, uint32_t init_val) {
    char name[32];
    if (copy_from_user(name, user_name, 31) < 0) return -EFAULT;
    name[31] = 0;

    uintptr_t fl = spin_lock_irqsave(&sem_table_lock);
    for (int i = 0; i < SEM_MAX; i++) {
        if (sem_table[i].active && strcmp(sem_table[i].name, name) == 0) {
            spin_unlock_irqrestore(&sem_table_lock, fl);
            return i;
        }
    }
    if (!(oflag & 0x40U)) { /* O_CREAT */
        spin_unlock_irqrestore(&sem_table_lock, fl);
        return -ENOENT;
    }
    for (int i = 0; i < SEM_MAX; i++) {
        if (!sem_table[i].active) {
            memset(&sem_table[i], 0, sizeof(sem_table[i]));
            sem_table[i].active = 1;
            strcpy(sem_table[i].name, name);
            sem_table[i].value = (int32_t)init_val;
            spin_unlock_irqrestore(&sem_table_lock, fl);
            return i;
        }
    }
    spin_unlock_irqrestore(&sem_table_lock, fl);
    return -ENOSPC;
}

static int syscall_sem_close_impl(int sid) {
    (void)sid;
    return 0;
}

static int syscall_sem_wait_impl(int sid) {
    if (sid < 0 || sid >= SEM_MAX) return -EINVAL;
    extern void process_sleep(uint32_t ticks);

    for (;;) {
        uintptr_t fl = spin_lock_irqsave(&sem_table[sid].lock);
        if (!sem_table[sid].active) {
            spin_unlock_irqrestore(&sem_table[sid].lock, fl);
            return -EINVAL;
        }
        if (sem_table[sid].value > 0) {
            sem_table[sid].value--;
            spin_unlock_irqrestore(&sem_table[sid].lock, fl);
            return 0;
        }
        spin_unlock_irqrestore(&sem_table[sid].lock, fl);
        process_sleep(1);
    }
}

static int syscall_sem_post_impl(int sid) {
    if (sid < 0 || sid >= SEM_MAX) return -EINVAL;
    uintptr_t fl = spin_lock_irqsave(&sem_table[sid].lock);
    if (!sem_table[sid].active) {
        spin_unlock_irqrestore(&sem_table[sid].lock, fl);
        return -EINVAL;
    }
    sem_table[sid].value++;
    spin_unlock_irqrestore(&sem_table[sid].lock, fl);
    return 0;
}

static int syscall_sem_unlink_impl(const char* user_name) {
    char name[32];
    if (copy_from_user(name, user_name, 31) < 0) return -EFAULT;
    name[31] = 0;

    uintptr_t fl = spin_lock_irqsave(&sem_table_lock);
    for (int i = 0; i < SEM_MAX; i++) {
        if (sem_table[i].active && strcmp(sem_table[i].name, name) == 0) {
            sem_table[i].active = 0;
            spin_unlock_irqrestore(&sem_table_lock, fl);
            return 0;
        }
    }
    spin_unlock_irqrestore(&sem_table_lock, fl);
    return -ENOENT;
}

static int syscall_sem_getvalue_impl(int sid, int* user_val) {
    if (sid < 0 || sid >= SEM_MAX) return -EINVAL;
    if (!user_val || user_range_ok(user_val, 4) == 0) return -EFAULT;
    uintptr_t fl = spin_lock_irqsave(&sem_table[sid].lock);
    if (!sem_table[sid].active) {
        spin_unlock_irqrestore(&sem_table[sid].lock, fl);
        return -EINVAL;
    }
    int32_t v = sem_table[sid].value;
    spin_unlock_irqrestore(&sem_table[sid].lock, fl);
    if (copy_to_user(user_val, &v, 4) < 0) return -EFAULT;
    return 0;
}

/* --- Shared library loading (dlopen/dlsym/dlclose) --- */
#define DLOPEN_MAX_LIBS 8
#define DLOPEN_MAX_SYMS 64
#define DLOPEN_BASE     0x30000000U
#define DLOPEN_STRIDE   0x00400000U  /* 4 MB per library */

struct dl_sym {
    char     name[64];
    uint32_t value;
};

struct dl_lib {
    int      active;
    char     path[128];
    uint32_t base;          /* load base address */
    struct dl_sym syms[DLOPEN_MAX_SYMS];
    uint32_t nsyms;
};

static struct dl_lib dl_table[DLOPEN_MAX_LIBS];
static spinlock_t dl_lock = {0};

static int syscall_dlopen_impl(const char* user_path) {
    char path[128];
    if (copy_from_user(path, user_path, 127) < 0) return -EFAULT;
    path[127] = 0;

    uintptr_t fl = spin_lock_irqsave(&dl_lock);

    /* Check if already loaded */
    for (int i = 0; i < DLOPEN_MAX_LIBS; i++) {
        if (dl_table[i].active && strcmp(dl_table[i].path, path) == 0) {
            spin_unlock_irqrestore(&dl_lock, fl);
            return i + 1; /* handle = 1-based index */
        }
    }

    /* Find free slot */
    int slot = -1;
    for (int i = 0; i < DLOPEN_MAX_LIBS; i++) {
        if (!dl_table[i].active) { slot = i; break; }
    }
    if (slot < 0) {
        spin_unlock_irqrestore(&dl_lock, fl);
        return -ENOMEM;
    }

    spin_unlock_irqrestore(&dl_lock, fl);

    /* Load the ELF .so file */
    extern fs_node_t* vfs_lookup(const char* path);
    fs_node_t* node = vfs_lookup(path);
    if (!node) return -ENOENT;

    uint32_t flen = node->length;
    if (flen < 52) return -EINVAL; /* minimum ELF header */

    extern void* kmalloc(size_t);
    extern void kfree(void*);
    uint8_t* fbuf = (uint8_t*)kmalloc(flen);
    if (!fbuf) return -ENOMEM;

    extern uint32_t vfs_read(fs_node_t*, uint32_t, uint32_t, uint8_t*);
    if (vfs_read(node, 0, flen, fbuf) != flen) {
        kfree(fbuf);
        return -EIO;
    }

    /* Basic ELF validation */
    if (fbuf[0] != 0x7F || fbuf[1] != 'E' || fbuf[2] != 'L' || fbuf[3] != 'F') {
        kfree(fbuf);
        return -EINVAL;
    }

    /* Load segments into current process address space at slot base */
    uint32_t base = DLOPEN_BASE + (uint32_t)slot * DLOPEN_STRIDE;

    /* Parse program headers and load PT_LOAD segments */
    uint32_t e_phoff = *(uint32_t*)(fbuf + 28);
    uint16_t e_phnum = *(uint16_t*)(fbuf + 44);
    uint16_t e_phentsize = *(uint16_t*)(fbuf + 42);

    if (e_phentsize < 32 || e_phoff + (uint32_t)e_phnum * e_phentsize > flen) {
        kfree(fbuf);
        return -EINVAL;
    }

    for (uint16_t i = 0; i < e_phnum; i++) {
        uint8_t* ph = fbuf + e_phoff + (uint32_t)i * e_phentsize;
        uint32_t p_type   = *(uint32_t*)(ph + 0);
        uint32_t p_offset = *(uint32_t*)(ph + 4);
        uint32_t p_vaddr  = *(uint32_t*)(ph + 8);
        uint32_t p_filesz = *(uint32_t*)(ph + 16);
        uint32_t p_memsz  = *(uint32_t*)(ph + 20);

        if (p_type != 1) continue; /* PT_LOAD = 1 */
        if (p_memsz == 0) continue;

        uint32_t vaddr = p_vaddr + base;
        if (vaddr >= 0xC0000000U) continue;

        /* Map pages */
        uint32_t start_page = vaddr & ~0xFFFU;
        uint32_t end_page = (vaddr + p_memsz - 1) & ~0xFFFU;
        for (uint32_t va = start_page; va <= end_page; va += 0x1000) {
            extern void* pmm_alloc_page(void);
            void* frame = pmm_alloc_page();
            if (!frame) { kfree(fbuf); return -ENOMEM; }
            vmm_map_page((uint64_t)(uintptr_t)frame, (uint64_t)va,
                         VMM_FLAG_PRESENT | VMM_FLAG_RW | VMM_FLAG_USER);
        }

        if (p_filesz && p_offset + p_filesz <= flen)
            memcpy((void*)vaddr, fbuf + p_offset, p_filesz);
        if (p_memsz > p_filesz)
            memset((void*)(vaddr + p_filesz), 0, p_memsz - p_filesz);
    }

    /* Extract symbols from .dynsym + .dynstr via PT_DYNAMIC */
    fl = spin_lock_irqsave(&dl_lock);
    memset(&dl_table[slot], 0, sizeof(dl_table[slot]));
    dl_table[slot].active = 1;
    strcpy(dl_table[slot].path, path);
    dl_table[slot].base = base;

    /* Parse PT_DYNAMIC to find SYMTAB and STRTAB */
    uint32_t symtab_va = 0, strtab_va = 0, strsz = 0;
    uint32_t hash_va = 0;
    for (uint16_t i = 0; i < e_phnum; i++) {
        uint8_t* ph = fbuf + e_phoff + (uint32_t)i * e_phentsize;
        uint32_t p_type   = *(uint32_t*)(ph + 0);
        uint32_t p_offset = *(uint32_t*)(ph + 4);
        uint32_t p_filesz = *(uint32_t*)(ph + 16);

        if (p_type != 2) continue; /* PT_DYNAMIC = 2 */
        if (p_offset + p_filesz > flen) break;

        uint32_t* dyn = (uint32_t*)(fbuf + p_offset);
        uint32_t dyn_entries = p_filesz / 8;
        for (uint32_t d = 0; d < dyn_entries; d++) {
            int32_t tag = (int32_t)dyn[d * 2];
            uint32_t val = dyn[d * 2 + 1];
            if (tag == 0) break; /* DT_NULL */
            if (tag == 6)  symtab_va = val + base; /* DT_SYMTAB */
            if (tag == 5)  strtab_va = val + base; /* DT_STRTAB */
            if (tag == 10) strsz = val;            /* DT_STRSZ */
            if (tag == 4)  hash_va = val + base;   /* DT_HASH */
        }
        break;
    }

    /* Read symbol count from DT_HASH if available: hash[1] = nchain = nsyms */
    uint32_t nsyms = 0;
    if (hash_va && hash_va < 0xC0000000U) {
        nsyms = *(uint32_t*)(hash_va + 4);
    }

    /* Extract exported symbols */
    if (symtab_va && strtab_va && nsyms > 0) {
        uint32_t cnt = 0;
        for (uint32_t s = 1; s < nsyms && cnt < DLOPEN_MAX_SYMS; s++) {
            uint32_t* sym = (uint32_t*)(symtab_va + s * 16);
            uint32_t st_name  = sym[0];
            uint32_t st_value = sym[1];
            uint8_t  st_info  = ((uint8_t*)sym)[12];
            uint16_t st_shndx = *(uint16_t*)((uint8_t*)sym + 14);

            /* Only global/weak defined symbols */
            uint8_t bind = st_info >> 4;
            if ((bind != 1 && bind != 2) || st_shndx == 0) continue;
            if (st_name >= strsz) continue;

            const char* name = (const char*)(strtab_va + st_name);
            if (name[0] == 0) continue;

            uint32_t nlen = 0;
            while (nlen < 63 && name[nlen]) nlen++;
            memcpy(dl_table[slot].syms[cnt].name, name, nlen);
            dl_table[slot].syms[cnt].name[nlen] = 0;
            dl_table[slot].syms[cnt].value = st_value + base;
            cnt++;
        }
        dl_table[slot].nsyms = cnt;
    }

    spin_unlock_irqrestore(&dl_lock, fl);
    kfree(fbuf);
    return slot + 1; /* 1-based handle */
}

static int syscall_dlsym_impl(int handle, const char* user_name, uint32_t* user_addr) {
    if (handle < 1 || handle > DLOPEN_MAX_LIBS) return -EINVAL;
    if (!user_name || !user_addr) return -EFAULT;
    if (user_range_ok(user_addr, 4) == 0) return -EFAULT;

    char name[64];
    if (copy_from_user(name, user_name, 63) < 0) return -EFAULT;
    name[63] = 0;

    int slot = handle - 1;
    uintptr_t fl = spin_lock_irqsave(&dl_lock);
    if (!dl_table[slot].active) {
        spin_unlock_irqrestore(&dl_lock, fl);
        return -EINVAL;
    }

    for (uint32_t i = 0; i < dl_table[slot].nsyms; i++) {
        if (strcmp(dl_table[slot].syms[i].name, name) == 0) {
            uint32_t addr = dl_table[slot].syms[i].value;
            spin_unlock_irqrestore(&dl_lock, fl);
            if (copy_to_user(user_addr, &addr, 4) < 0) return -EFAULT;
            return 0;
        }
    }

    spin_unlock_irqrestore(&dl_lock, fl);
    return -ENOENT;
}

static int syscall_dlclose_impl(int handle) {
    if (handle < 1 || handle > DLOPEN_MAX_LIBS) return -EINVAL;
    int slot = handle - 1;
    uintptr_t fl = spin_lock_irqsave(&dl_lock);
    if (!dl_table[slot].active) {
        spin_unlock_irqrestore(&dl_lock, fl);
        return -EINVAL;
    }
    dl_table[slot].active = 0;
    spin_unlock_irqrestore(&dl_lock, fl);
    return 0;
}

/* --- Advisory file locking (flock) --- */
enum {
    FLOCK_SH = 1,
    FLOCK_EX = 2,
    FLOCK_NB = 4,
    FLOCK_UN = 8,
};

#define FLOCK_TABLE_SIZE 64

struct flock_entry {
    uint32_t inode;
    uint32_t pid;
    int      type;      /* FLOCK_SH or FLOCK_EX */
    int      active;
};

static struct flock_entry flock_table[FLOCK_TABLE_SIZE];
static spinlock_t flock_lock_g = {0};

static int flock_can_acquire(uint32_t inode, uint32_t pid, int type) {
    for (int i = 0; i < FLOCK_TABLE_SIZE; i++) {
        if (!flock_table[i].active || flock_table[i].inode != inode)
            continue;
        if (flock_table[i].pid == pid)
            continue; /* our own lock — will be upgraded/downgraded */
        if (type == FLOCK_EX || flock_table[i].type == FLOCK_EX)
            return 0; /* conflict */
    }
    return 1;
}

static int flock_do(uint32_t inode, uint32_t pid, int operation) {
    int type = operation & (FLOCK_SH | FLOCK_EX);
    int nonblock = operation & FLOCK_NB;

    if (operation & FLOCK_UN) {
        uintptr_t fl = spin_lock_irqsave(&flock_lock_g);
        for (int i = 0; i < FLOCK_TABLE_SIZE; i++) {
            if (flock_table[i].active && flock_table[i].inode == inode &&
                flock_table[i].pid == pid) {
                flock_table[i].active = 0;
                break;
            }
        }
        spin_unlock_irqrestore(&flock_lock_g, fl);
        return 0;
    }

    if (!type) return -EINVAL;

    for (;;) {
        uintptr_t fl = spin_lock_irqsave(&flock_lock_g);

        if (flock_can_acquire(inode, pid, type)) {
            /* Find existing entry for this pid+inode or allocate new */
            int slot = -1;
            int free_slot = -1;
            for (int i = 0; i < FLOCK_TABLE_SIZE; i++) {
                if (flock_table[i].active && flock_table[i].inode == inode &&
                    flock_table[i].pid == pid) {
                    slot = i;
                    break;
                }
                if (!flock_table[i].active && free_slot < 0)
                    free_slot = i;
            }
            if (slot >= 0) {
                flock_table[slot].type = type; /* upgrade/downgrade */
            } else if (free_slot >= 0) {
                flock_table[free_slot].inode = inode;
                flock_table[free_slot].pid = pid;
                flock_table[free_slot].type = type;
                flock_table[free_slot].active = 1;
            } else {
                spin_unlock_irqrestore(&flock_lock_g, fl);
                return -ENOLCK;
            }
            spin_unlock_irqrestore(&flock_lock_g, fl);
            return 0;
        }

        spin_unlock_irqrestore(&flock_lock_g, fl);

        if (nonblock) return -EWOULDBLOCK;

        extern void process_sleep(uint32_t ticks);
        process_sleep(1); /* block and retry */
    }
}

static void flock_release_pid(uint32_t pid) {
    uintptr_t fl = spin_lock_irqsave(&flock_lock_g);
    for (int i = 0; i < FLOCK_TABLE_SIZE; i++) {
        if (flock_table[i].active && flock_table[i].pid == pid)
            flock_table[i].active = 0;
    }
    spin_unlock_irqrestore(&flock_lock_g, fl);
}

enum {
    FCNTL_F_DUPFD = 0,
    FCNTL_F_GETFD = 1,
    FCNTL_F_SETFD = 2,
    FCNTL_F_GETFL = 3,
    FCNTL_F_SETFL = 4,
    FCNTL_F_GETLK = 5,
    FCNTL_F_SETLK = 6,
    FCNTL_F_SETLKW = 7,
    FCNTL_F_DUPFD_CLOEXEC = 1030,
    FCNTL_F_GETPIPE_SZ = 1032,
    FCNTL_F_SETPIPE_SZ = 1033,
};

enum {
    F_RDLCK = 0,
    F_WRLCK = 1,
    F_UNLCK = 2,
};

struct k_flock {
    int16_t  l_type;
    int16_t  l_whence;
    uint32_t l_start;
    uint32_t l_len;     /* 0 = to EOF */
    uint32_t l_pid;
};

#define RLOCK_TABLE_SIZE 64

struct rlock_entry {
    uint32_t inode;
    uint32_t pid;
    uint32_t start;
    uint32_t end;       /* 0xFFFFFFFF = to EOF */
    int      type;      /* F_RDLCK or F_WRLCK */
    int      active;
};

static struct rlock_entry rlock_table[RLOCK_TABLE_SIZE];
static spinlock_t rlock_lock_g = {0};

static int rlock_overlaps(uint32_t s1, uint32_t e1, uint32_t s2, uint32_t e2) {
    return s1 <= e2 && s2 <= e1;
}

static int rlock_conflicts(uint32_t inode, uint32_t pid, int type,
                           uint32_t start, uint32_t end, struct rlock_entry** out) {
    for (int i = 0; i < RLOCK_TABLE_SIZE; i++) {
        struct rlock_entry* e = &rlock_table[i];
        if (!e->active || e->inode != inode) continue;
        if (e->pid == pid) continue;
        if (!rlock_overlaps(start, end, e->start, e->end)) continue;
        if (type == F_WRLCK || e->type == F_WRLCK) {
            if (out) *out = e;
            return 1;
        }
    }
    return 0;
}

static int rlock_setlk(uint32_t inode, uint32_t pid, int type,
                        uint32_t start, uint32_t end, int blocking) {
    if (type == F_UNLCK) {
        uintptr_t fl = spin_lock_irqsave(&rlock_lock_g);
        for (int i = 0; i < RLOCK_TABLE_SIZE; i++) {
            struct rlock_entry* e = &rlock_table[i];
            if (e->active && e->inode == inode && e->pid == pid &&
                rlock_overlaps(start, end, e->start, e->end)) {
                e->active = 0;
            }
        }
        spin_unlock_irqrestore(&rlock_lock_g, fl);
        return 0;
    }

    for (;;) {
        uintptr_t fl = spin_lock_irqsave(&rlock_lock_g);

        if (!rlock_conflicts(inode, pid, type, start, end, NULL)) {
            /* Remove our own overlapping locks, then insert */
            int slot = -1;
            for (int i = 0; i < RLOCK_TABLE_SIZE; i++) {
                struct rlock_entry* e = &rlock_table[i];
                if (e->active && e->inode == inode && e->pid == pid &&
                    rlock_overlaps(start, end, e->start, e->end)) {
                    e->active = 0;
                }
                if (!e->active && slot < 0) slot = i;
            }
            if (slot < 0) {
                /* Scan again for free slot after removals */
                for (int i = 0; i < RLOCK_TABLE_SIZE; i++) {
                    if (!rlock_table[i].active) { slot = i; break; }
                }
            }
            if (slot < 0) {
                spin_unlock_irqrestore(&rlock_lock_g, fl);
                return -ENOLCK;
            }
            rlock_table[slot].inode = inode;
            rlock_table[slot].pid = pid;
            rlock_table[slot].start = start;
            rlock_table[slot].end = end;
            rlock_table[slot].type = type;
            rlock_table[slot].active = 1;
            spin_unlock_irqrestore(&rlock_lock_g, fl);
            return 0;
        }

        spin_unlock_irqrestore(&rlock_lock_g, fl);
        if (!blocking) return -EAGAIN;

        extern void process_sleep(uint32_t ticks);
        process_sleep(1);
    }
}

static void rlock_release_pid(uint32_t pid) {
    uintptr_t fl = spin_lock_irqsave(&rlock_lock_g);
    for (int i = 0; i < RLOCK_TABLE_SIZE; i++) {
        if (rlock_table[i].active && rlock_table[i].pid == pid)
            rlock_table[i].active = 0;
    }
    spin_unlock_irqrestore(&rlock_lock_g, fl);
}

enum {
    AT_FDCWD = -100,
};

static int path_resolve_user(const char* user_path, char* out, size_t out_sz);

static int fd_alloc(struct file* f);
static int fd_close(int fd);
static struct file* fd_get(int fd);
static void socket_syscall_dispatch(struct registers* regs, uint32_t syscall_no);
static void posix_ext_syscall_dispatch(struct registers* regs, uint32_t syscall_no);

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
    arch_regs_set_retval(&child_regs, 0);

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

__attribute__((noinline))
static int syscall_clone_impl(struct registers* regs) {
    if (!regs || !current_process) return -EINVAL;

    uint32_t clone_flags = sc_arg0(regs);
    uintptr_t child_stack = (uintptr_t)sc_arg1(regs);
    uintptr_t tls_base = (uintptr_t)sc_arg3(regs);

    struct process* child = process_clone_create(clone_flags, child_stack, regs, tls_base);
    if (!child) return -ENOMEM;

    /* CLONE_PARENT_SETTID: write child tid to parent user address */
    if ((clone_flags & CLONE_PARENT_SETTID) && sc_arg2(regs)) {
        uint32_t tid = child->pid;
        (void)copy_to_user((void*)(uintptr_t)sc_arg2(regs), &tid, sizeof(tid));
    }

    /* CLONE_CHILD_CLEARTID: store the address for the child to clear on exit */
    if ((clone_flags & CLONE_CHILD_CLEARTID) && sc_arg4(regs)) {
        child->clear_child_tid = (uint32_t*)(uintptr_t)sc_arg4(regs);
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
                ready++;
                continue;
            }

            fs_node_t* n = f->node;

            int (*fn_poll)(fs_node_t*, int) = NULL;
            if (n->f_ops && n->f_ops->poll) fn_poll = n->f_ops->poll;
            if (fn_poll) {
                int vfs_events = 0;
                if (kfds[i].events & POLLIN)  vfs_events |= VFS_POLL_IN;
                if (kfds[i].events & POLLOUT) vfs_events |= VFS_POLL_OUT;

                int vfs_rev = fn_poll(n, vfs_events);

                if (vfs_rev & VFS_POLL_IN)  kfds[i].revents |= POLLIN;
                if (vfs_rev & VFS_POLL_OUT) kfds[i].revents |= POLLOUT;
                if (vfs_rev & VFS_POLL_ERR) kfds[i].revents |= POLLERR;
                if (vfs_rev & VFS_POLL_HUP) kfds[i].revents |= POLLHUP;
            } else {
                if (kfds[i].events & POLLIN)  kfds[i].revents |= POLLIN;
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

static int pipe_poll(fs_node_t* n, int events) {
    struct pipe_node* pn = (struct pipe_node*)n;
    if (!pn || !pn->ps) return VFS_POLL_ERR;
    struct pipe_state* ps = pn->ps;
    int revents = 0;
    if (pn->is_read_end) {
        if ((events & VFS_POLL_IN) && (ps->count > 0 || ps->writers == 0)) {
            revents |= VFS_POLL_IN;
            if (ps->writers == 0) revents |= VFS_POLL_HUP;
        }
    } else {
        if (ps->readers == 0) {
            if (events & VFS_POLL_OUT) revents |= VFS_POLL_ERR;
        } else {
            uint32_t free = ps->cap - ps->count;
            if ((events & VFS_POLL_OUT) && free > 0) revents |= VFS_POLL_OUT;
        }
    }
    return revents;
}

static const struct file_operations pipe_read_fops = {
    .read  = pipe_read,
    .close = pipe_close,
    .poll  = pipe_poll,
};

static const struct file_operations pipe_write_fops = {
    .write = pipe_write,
    .close = pipe_close,
    .poll  = pipe_poll,
};

static int pipe_node_create(struct pipe_state* ps, int is_read_end, fs_node_t** out_node) {
    if (!ps || !out_node) return -EINVAL;
    struct pipe_node* pn = (struct pipe_node*)kmalloc(sizeof(*pn));
    if (!pn) return -ENOMEM;
    memset(pn, 0, sizeof(*pn));

    pn->ps = ps;
    pn->is_read_end = is_read_end ? 1U : 0U;
    pn->node.flags = FS_FILE;
    pn->node.length = 0;
    if (pn->is_read_end) {
        strcpy(pn->node.name, "pipe:r");
        pn->node.f_ops = &pipe_read_fops;
        ps->readers++;
    } else {
        strcpy(pn->node.name, "pipe:w");
        pn->node.f_ops = &pipe_write_fops;
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
    st->st_uid = node->uid;
    st->st_gid = node->gid;

    uint32_t mode = node->mode & 07777;
    if (node->flags == FS_DIRECTORY) mode |= S_IFDIR;
    else if (node->flags == FS_CHARDEVICE) mode |= S_IFCHR;
    else if (node->flags == FS_SYMLINK) mode |= S_IFLNK;
    else mode |= S_IFREG;
    if ((mode & 07777) == 0) mode |= 0755;
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

    sc_ip(regs) = (uint32_t)entry;
    sc_usp(regs) = (uint32_t)sp;
    sc_ret(regs) = 0;
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

/*
 * Check if the current process has the requested access to a file node.
 * want: bitmask of 4 (read), 2 (write), 1 (execute).
 * Returns 0 if allowed, -EACCES if denied.
 */
static int vfs_check_permission(fs_node_t* node, int want) {
    if (!current_process) return 0;       /* kernel context — allow all */
    if (current_process->euid == 0) return 0;  /* root — allow all */
    if (node->mode == 0) return 0;        /* mode not set — permissive */

    uint32_t mode = node->mode;
    uint32_t perm;

    if (current_process->euid == node->uid) {
        perm = (mode >> 6) & 7;  /* owner bits */
    } else if (current_process->egid == node->gid) {
        perm = (mode >> 3) & 7;  /* group bits */
    } else {
        perm = mode & 7;         /* other bits */
    }

    if ((want & perm) != (uint32_t)want) return -EACCES;
    return 0;
}

static int syscall_open_impl(const char* user_path, uint32_t flags) {
    if (!user_path) return -EFAULT;

    char path[128];
    int prc = path_resolve_user(user_path, path, sizeof(path));
    if (prc < 0) return prc;

    fs_node_t* node = vfs_lookup(path);
    if (!node && (flags & 0x40U) != 0U) {
        /* O_CREAT: create file through VFS */
        int rc = vfs_create(path, flags, &node);
        if (rc < 0) return rc;
    } else if (!node) {
        return -ENOENT;
    } else if ((flags & 0x200U) != 0U && node->flags == FS_FILE) {
        /* O_TRUNC on existing file */
        if (node->i_ops && node->i_ops->truncate) {
            node->i_ops->truncate(node, 0);
            node->length = 0;
        }
    }

    /* Permission check based on open flags */
    {
        int want = 4; /* default: read */
        uint32_t acc = flags & 3U; /* O_RDONLY=0, O_WRONLY=1, O_RDWR=2 */
        if (acc == 1) want = 2;        /* write only */
        else if (acc == 2) want = 6;   /* read + write */
        int perm_rc = vfs_check_permission(node, want);
        if (perm_rc < 0) return perm_rc;
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
        uint32_t keep = f->flags & ~(O_NONBLOCK | O_APPEND);
        uint32_t set = arg & (O_NONBLOCK | O_APPEND);
        f->flags = keep | set;
        return 0;
    }
    if (cmd == FCNTL_F_GETLK || cmd == FCNTL_F_SETLK || cmd == FCNTL_F_SETLKW) {
        if (!current_process || !f->node) return -EINVAL;
        void* user_fl = (void*)(uintptr_t)arg;
        if (!user_fl || user_range_ok(user_fl, sizeof(struct k_flock)) == 0)
            return -EFAULT;

        struct k_flock kfl;
        if (copy_from_user(&kfl, user_fl, sizeof(kfl)) < 0) return -EFAULT;

        uint32_t ino = f->node->inode;
        uint32_t start = kfl.l_start;
        uint32_t end = (kfl.l_len == 0) ? 0xFFFFFFFFU : start + kfl.l_len - 1;

        if (cmd == FCNTL_F_GETLK) {
            uintptr_t fl = spin_lock_irqsave(&rlock_lock_g);
            struct rlock_entry* conflict = NULL;
            int has = rlock_conflicts(ino, current_process->pid,
                                      kfl.l_type, start, end, &conflict);
            if (has && conflict) {
                kfl.l_type = (int16_t)conflict->type;
                kfl.l_whence = 0; /* SEEK_SET */
                kfl.l_start = conflict->start;
                kfl.l_len = (conflict->end == 0xFFFFFFFFU) ? 0
                            : conflict->end - conflict->start + 1;
                kfl.l_pid = conflict->pid;
            } else {
                kfl.l_type = F_UNLCK;
            }
            spin_unlock_irqrestore(&rlock_lock_g, fl);
            if (copy_to_user(user_fl, &kfl, sizeof(kfl)) < 0) return -EFAULT;
            return 0;
        }

        return rlock_setlk(ino, current_process->pid, kfl.l_type,
                           start, end, cmd == FCNTL_F_SETLKW);
    }
    if (cmd == FCNTL_F_GETPIPE_SZ) {
        if (!f->node) return -EBADF;
        if (f->node->f_ops != &pipe_read_fops && f->node->f_ops != &pipe_write_fops)
            return -ENOTTY;
        struct pipe_node* pn = (struct pipe_node*)f->node;
        return (int)pn->ps->cap;
    }
    if (cmd == FCNTL_F_SETPIPE_SZ) {
        if (!f->node) return -EBADF;
        if (f->node->f_ops != &pipe_read_fops && f->node->f_ops != &pipe_write_fops)
            return -ENOTTY;
        struct pipe_node* pn = (struct pipe_node*)f->node;
        struct pipe_state* ps = pn->ps;
        uint32_t new_cap = arg;
        if (new_cap < 512) new_cap = 512;
        if (new_cap > 65536) new_cap = 65536;
        if (new_cap == ps->cap) return (int)ps->cap;
        if (new_cap < ps->count) return -EBUSY;
        uint8_t* new_buf = (uint8_t*)kmalloc(new_cap);
        if (!new_buf) return -ENOMEM;
        for (uint32_t i = 0; i < ps->count; i++) {
            new_buf[i] = ps->buf[(ps->rpos + i) % ps->cap];
        }
        kfree(ps->buf);
        ps->buf = new_buf;
        ps->rpos = 0;
        ps->wpos = ps->count;
        ps->cap = new_cap;
        return (int)ps->cap;
    }
    if (cmd == FCNTL_F_DUPFD_CLOEXEC) {
        if (!current_process) return -EINVAL;
        int new_fd = -1;
        for (int i = (int)arg; i < PROCESS_MAX_FILES; i++) {
            if (!current_process->files[i]) { new_fd = i; break; }
        }
        if (new_fd < 0) return -EMFILE;
        current_process->files[new_fd] = f;
        f->refcount++;
        current_process->fd_flags[new_fd] = FD_CLOEXEC;
        return new_fd;
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

    return vfs_mkdir(path);
}

static int syscall_getdents_impl(int fd, void* user_buf, uint32_t len) {
    if (len == 0) return 0;
    if (!user_buf) return -EFAULT;
    if (user_range_ok(user_buf, (size_t)len) == 0) return -EFAULT;

    struct file* f = fd_get(fd);
    if (!f || !f->node) return -EBADF;
    if (f->node->flags != FS_DIRECTORY) return -ENOTDIR;
    int (*fn_readdir)(struct fs_node*, uint32_t*, void*, uint32_t) = NULL;
    if (f->node->i_ops && f->node->i_ops->readdir) fn_readdir = f->node->i_ops->readdir;
    if (!fn_readdir) return -ENOSYS;

    uint8_t kbuf[256];
    uint32_t klen = len;
    if (klen > (uint32_t)sizeof(kbuf)) klen = (uint32_t)sizeof(kbuf);

    uint32_t idx = f->offset;
    int rc = fn_readdir(f->node, &idx, kbuf, klen);
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

    return vfs_unlink(path);
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

    return vfs_rmdir(path);
}

static int syscall_rename_impl(const char* user_old, const char* user_new) {
    if (!user_old || !user_new) return -EFAULT;

    char oldp[128];
    char newp[128];
    int rc = path_resolve_user(user_old, oldp, sizeof(oldp));
    if (rc < 0) return rc;
    rc = path_resolve_user(user_new, newp, sizeof(newp));
    if (rc < 0) return rc;

    return vfs_rename(oldp, newp);
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
    {
        int (*fn_poll)(fs_node_t*, int) = NULL;
        if (f->node->f_ops && f->node->f_ops->poll) fn_poll = f->node->f_ops->poll;
        if (nonblock && fn_poll) {
            int rev = fn_poll(f->node, VFS_POLL_IN);
            if (!(rev & (VFS_POLL_IN | VFS_POLL_ERR | VFS_POLL_HUP)))
                return -EAGAIN;
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

    if (!(f->node->f_ops && f->node->f_ops->read)) return -ESPIPE;

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
    {
        int (*fn_poll)(fs_node_t*, int) = NULL;
        if (f->node->f_ops && f->node->f_ops->poll) fn_poll = f->node->f_ops->poll;
        if (nonblock && fn_poll) {
            int rev = fn_poll(f->node, VFS_POLL_OUT);
            if (!(rev & (VFS_POLL_OUT | VFS_POLL_ERR)))
                return -EAGAIN;
        }
    }
    if (!(f->node->f_ops && f->node->f_ops->write)) return -ESPIPE;
    if (f->node->flags != FS_FILE && f->node->flags != FS_CHARDEVICE && f->node->flags != FS_SOCKET) return -ESPIPE;

    if ((f->flags & O_APPEND) && (f->node->flags & FS_FILE)) {
        f->offset = f->node->length;
    }

    uint8_t kbuf[1024];
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
    if (n->f_ops && n->f_ops->ioctl) return n->f_ops->ioctl(n, cmd, user_arg);
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

    uint32_t ms = req.tv_sec * 1000U + req.tv_nsec / 1000000U;
    uint32_t ticks = (ms + TIMER_MS_PER_TICK - 1) / TIMER_MS_PER_TICK;
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

    struct timespec tp;
    if (clk_id == CLOCK_REALTIME) {
        tp.tv_sec = rtc_unix_timestamp();
        tp.tv_nsec = 0;
    } else {
        uint32_t ticks = get_tick_count();
        uint32_t total_ms = ticks * TIMER_MS_PER_TICK;
        tp.tv_sec = total_ms / 1000U;
        tp.tv_nsec = (total_ms % 1000U) * 1000000U;
    }

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

    return vmm_find_free_area(MMAP_BASE, MMAP_END, length);
}

__attribute__((noinline))
static uintptr_t syscall_mmap_impl(uintptr_t addr, uint32_t length, uint32_t prot,
                                    uint32_t flags, int fd, uint32_t offset) {
    if (!current_process) return (uintptr_t)-EINVAL;
    if (length == 0) return (uintptr_t)-EINVAL;

    int is_anon = (flags & MAP_ANONYMOUS) != 0;

    /* fd-backed mmap: the file's node must provide a mmap callback */
    fs_node_t* mmap_node = NULL;
    if (!is_anon) {
        if (fd < 0) return (uintptr_t)-EBADF;
        struct file* f = fd_get(fd);
        if (!f || !f->node) return (uintptr_t)-EBADF;
        if (!(f->node->f_ops && f->node->f_ops->mmap)) return (uintptr_t)-ENOSYS;
        mmap_node = f->node;
    }

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

    if (mmap_node) {
        /* Device-backed mmap: delegate to the node's mmap callback */
        uintptr_t (*fn_mmap)(fs_node_t*, uintptr_t, uint32_t, uint32_t, uint32_t) = NULL;
        if (mmap_node->f_ops && mmap_node->f_ops->mmap) fn_mmap = mmap_node->f_ops->mmap;
        if (!fn_mmap) return (uintptr_t)-ENOSYS;
        uintptr_t result = fn_mmap(mmap_node, base, aligned_len, prot, offset);
        if (!result) return (uintptr_t)-ENOMEM;
        base = result;
    } else {
        /* Anonymous mmap: allocate fresh zeroed pages */
        uint32_t vmm_flags = VMM_FLAG_PRESENT | VMM_FLAG_USER;
        if (prot & PROT_WRITE) vmm_flags |= VMM_FLAG_RW;

        for (uintptr_t va = base; va < base + aligned_len; va += 0x1000U) {
            void* frame = pmm_alloc_page();
            if (!frame) return (uintptr_t)-ENOMEM;
            vmm_map_page((uint64_t)(uintptr_t)frame, (uint64_t)va, vmm_flags);
            memset((void*)va, 0, 0x1000U);
        }
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

static int syscall_symlink_impl(const char* user_target, const char* user_linkpath) {
    if (!user_target || !user_linkpath) return -EFAULT;

    char target[128], linkpath[128];
    if (copy_from_user(target, user_target, sizeof(target)) < 0) return -EFAULT;
    target[sizeof(target) - 1] = 0;

    int prc = path_resolve_user(user_linkpath, linkpath, sizeof(linkpath));
    if (prc < 0) return prc;

    /* Find parent directory */
    char parent[128];
    char leaf[128];
    strcpy(parent, linkpath);
    char* last_slash = NULL;
    for (char* p = parent; *p; p++) {
        if (*p == '/') last_slash = p;
    }
    if (!last_slash) return -EINVAL;
    if (last_slash == parent) {
        parent[1] = 0;
        strcpy(leaf, linkpath + 1);
    } else {
        *last_slash = 0;
        strcpy(leaf, last_slash + 1);
    }
    if (leaf[0] == 0) return -EINVAL;

    fs_node_t* dir = vfs_lookup(parent);
    if (!dir || dir->flags != FS_DIRECTORY) return -ENOENT;

    return tmpfs_create_symlink(dir, leaf, target);
}

static int syscall_readlink_impl(const char* user_path, char* user_buf, uint32_t bufsz) {
    if (!user_path || !user_buf) return -EFAULT;
    if (bufsz == 0) return -EINVAL;
    if (user_range_ok(user_buf, (size_t)bufsz) == 0) return -EFAULT;

    char path[128];
    int prc = path_resolve_user(user_path, path, sizeof(path));
    if (prc < 0) return prc;

    /* readlink must NOT follow the final symlink — look up parent + finddir */
    char parent[128];
    char leaf[128];
    strcpy(parent, path);
    char* last_slash = NULL;
    for (char* p = parent; *p; p++) {
        if (*p == '/') last_slash = p;
    }
    if (!last_slash) return -EINVAL;
    if (last_slash == parent) {
        parent[1] = 0;
        strcpy(leaf, path + 1);
    } else {
        *last_slash = 0;
        strcpy(leaf, last_slash + 1);
    }

    fs_node_t* dir = vfs_lookup(parent);
    if (!dir) return -ENOENT;
    fs_node_t* (*fn_finddir)(fs_node_t*, const char*) = NULL;
    if (dir->i_ops && dir->i_ops->lookup) fn_finddir = dir->i_ops->lookup;
    if (!fn_finddir) return -ENOENT;

    fs_node_t* node = fn_finddir(dir, leaf);
    if (!node) return -ENOENT;
    if (node->flags != FS_SYMLINK) return -EINVAL;

    uint32_t len = (uint32_t)strlen(node->symlink_target);
    if (len > bufsz) len = bufsz;
    if (copy_to_user(user_buf, node->symlink_target, len) < 0) return -EFAULT;
    return (int)len;
}

static int syscall_link_impl(const char* user_oldpath, const char* user_newpath) {
    if (!user_oldpath || !user_newpath) return -EFAULT;
    char old_path[128], new_path[128];
    int rc1 = path_resolve_user(user_oldpath, old_path, sizeof(old_path));
    if (rc1 < 0) return rc1;
    int rc2 = path_resolve_user(user_newpath, new_path, sizeof(new_path));
    if (rc2 < 0) return rc2;
    return vfs_link(old_path, new_path);
}

static int syscall_chmod_impl(const char* user_path, uint32_t mode) {
    if (!user_path) return -EFAULT;

    char path[128];
    int prc = path_resolve_user(user_path, path, sizeof(path));
    if (prc < 0) return prc;

    fs_node_t* node = vfs_lookup(path);
    if (!node) return -ENOENT;

    /* Only root or file owner can chmod */
    if (current_process && current_process->euid != 0 &&
        current_process->euid != node->uid) {
        return -EPERM;
    }

    node->mode = mode & 07777;
    return 0;
}

static int syscall_chown_impl(const char* user_path, uint32_t uid, uint32_t gid) {
    if (!user_path) return -EFAULT;

    char path[128];
    int prc = path_resolve_user(user_path, path, sizeof(path));
    if (prc < 0) return prc;

    fs_node_t* node = vfs_lookup(path);
    if (!node) return -ENOENT;

    /* Only root can chown */
    if (current_process && current_process->euid != 0) {
        return -EPERM;
    }

    node->uid = uid;
    node->gid = gid;
    return 0;
}

void syscall_handler(struct registers* regs) {
    uint32_t syscall_no = sc_num(regs);

    if (syscall_no == SYSCALL_WRITE) {
        uint32_t fd = sc_arg0(regs);
        const char* buf = (const char*)sc_arg1(regs);
        uint32_t len = sc_arg2(regs);

        sc_ret(regs) = (uint32_t)syscall_write_impl((int)fd, buf, len);
        return;
    }

    if (syscall_no == SYSCALL_GETPID) {
        sc_ret(regs) = current_process ? current_process->pid : 0;
        return;
    }

    if (syscall_no == SYSCALL_GETPPID) {
        sc_ret(regs) = current_process ? current_process->parent_pid : 0;
        return;
    }

    if (syscall_no == SYSCALL_OPEN) {
        const char* path = (const char*)sc_arg0(regs);
        uint32_t flags = sc_arg1(regs);
        sc_ret(regs) = (uint32_t)syscall_open_impl(path, flags);
        return;
    }

    if (syscall_no == SYSCALL_OPENAT) {
        int dirfd = (int)sc_arg0(regs);
        const char* path = (const char*)sc_arg1(regs);
        uint32_t flags = (uint32_t)sc_arg2(regs);
        uint32_t mode = (uint32_t)sc_arg3(regs);
        sc_ret(regs) = (uint32_t)syscall_openat_impl(dirfd, path, flags, mode);
        return;
    }

    if (syscall_no == SYSCALL_CHDIR) {
        const char* path = (const char*)sc_arg0(regs);
        sc_ret(regs) = (uint32_t)syscall_chdir_impl(path);
        return;
    }

    if (syscall_no == SYSCALL_GETCWD) {
        char* buf = (char*)sc_arg0(regs);
        uint32_t size = (uint32_t)sc_arg1(regs);
        sc_ret(regs) = (uint32_t)syscall_getcwd_impl(buf, size);
        return;
    }

    if (syscall_no == SYSCALL_READ) {
        int fd = (int)sc_arg0(regs);
        void* buf = (void*)sc_arg1(regs);
        uint32_t len = sc_arg2(regs);
        sc_ret(regs) = (uint32_t)syscall_read_impl(fd, buf, len);
        return;
    }

    if (syscall_no == SYSCALL_CLOSE) {
        int fd = (int)sc_arg0(regs);
        sc_ret(regs) = (uint32_t)fd_close(fd);
        return;
    }

    if (syscall_no == SYSCALL_EXIT) {
        int status = (int)sc_arg0(regs);

        if (current_process) {
            flock_release_pid(current_process->pid);
            rlock_release_pid(current_process->pid);
        }

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
        int pid = (int)sc_arg0(regs);
        int* user_status = (int*)sc_arg1(regs);
        uint32_t options = sc_arg2(regs);

        if (user_status && user_range_ok(user_status, sizeof(int)) == 0) {
            sc_ret(regs) = (uint32_t)-EFAULT;
            return;
        }

        int status = 0;
        int retpid = process_waitpid(pid, &status, options);
        if (retpid < 0) {
            sc_ret(regs) = (uint32_t)retpid;
            return;
        }

        if (retpid == 0) {
            sc_ret(regs) = 0;
            return;
        }

        if (user_status) {
            if (copy_to_user(user_status, &status, sizeof(status)) < 0) {
                sc_ret(regs) = (uint32_t)-EFAULT;
                return;
            }
        }

        sc_ret(regs) = (uint32_t)retpid;
        return;
    }

    if (syscall_no == SYSCALL_WAITID) {
        /* waitid(idtype, id, siginfo_t* infop, options)
         * idtype: 0=P_ALL, 1=P_PID, 2=P_PGID */
        uint32_t idtype = sc_arg0(regs);
        uint32_t id = sc_arg1(regs);
        void* user_infop = (void*)sc_arg2(regs);
        uint32_t options = (uint32_t)((int32_t)sc_arg3(regs));

        int wait_pid_arg;
        if (idtype == 0) wait_pid_arg = -1;        /* P_ALL */
        else if (idtype == 1) wait_pid_arg = (int)id; /* P_PID */
        else { sc_ret(regs) = (uint32_t)-EINVAL; return; }

        if (user_infop && user_range_ok(user_infop, 16) == 0) {
            sc_ret(regs) = (uint32_t)-EFAULT;
            return;
        }

        int status = 0;
        int retpid = process_waitpid(wait_pid_arg, &status, options);
        if (retpid < 0) {
            sc_ret(regs) = (uint32_t)retpid;
            return;
        }
        if (retpid == 0) {
            /* WNOHANG, no child changed state yet */
            if (user_infop) {
                uint32_t zero[4] = {0, 0, 0, 0};
                (void)copy_to_user(user_infop, zero, 16);
            }
            sc_ret(regs) = 0;
            return;
        }
        if (user_infop) {
            /* Fill minimal siginfo: si_signo=SIGCHLD(17), si_code=CLD_EXITED(1),
             * si_pid, si_status */
            uint32_t info[4];
            info[0] = 17;           /* si_signo = SIGCHLD */
            info[1] = 1;            /* si_code = CLD_EXITED */
            info[2] = (uint32_t)retpid; /* si_pid */
            info[3] = (uint32_t)status; /* si_status */
            if (copy_to_user(user_infop, info, 16) < 0) {
                sc_ret(regs) = (uint32_t)-EFAULT;
                return;
            }
        }
        sc_ret(regs) = 0;
        return;
    }

    if (syscall_no == SYSCALL_LSEEK) {
        int fd = (int)sc_arg0(regs);
        int32_t off = (int32_t)sc_arg1(regs);
        int whence = (int)sc_arg2(regs);
        sc_ret(regs) = (uint32_t)syscall_lseek_impl(fd, off, whence);
        return;
    }

    if (syscall_no == SYSCALL_FSTAT) {
        int fd = (int)sc_arg0(regs);
        struct stat* st = (struct stat*)sc_arg1(regs);
        sc_ret(regs) = (uint32_t)syscall_fstat_impl(fd, st);
        return;
    }

    if (syscall_no == SYSCALL_STAT) {
        const char* path = (const char*)sc_arg0(regs);
        struct stat* user_st = (struct stat*)sc_arg1(regs);
        sc_ret(regs) = (uint32_t)syscall_stat_impl(path, user_st);
        return;
    }

    if (syscall_no == SYSCALL_FSTATAT) {
        int dirfd = (int)sc_arg0(regs);
        const char* path = (const char*)sc_arg1(regs);
        struct stat* user_st = (struct stat*)sc_arg2(regs);
        uint32_t flags = (uint32_t)sc_arg3(regs);
        sc_ret(regs) = (uint32_t)syscall_fstatat_impl(dirfd, path, user_st, flags);
        return;
    }

    if (syscall_no == SYSCALL_DUP) {
        int oldfd = (int)sc_arg0(regs);
        sc_ret(regs) = (uint32_t)syscall_dup_impl(oldfd);
        return;
    }

    if (syscall_no == SYSCALL_DUP2) {
        int oldfd = (int)sc_arg0(regs);
        int newfd = (int)sc_arg1(regs);
        sc_ret(regs) = (uint32_t)syscall_dup2_impl(oldfd, newfd);
        return;
    }

    if (syscall_no == SYSCALL_DUP3) {
        int oldfd = (int)sc_arg0(regs);
        int newfd = (int)sc_arg1(regs);
        uint32_t flags = (uint32_t)sc_arg2(regs);
        sc_ret(regs) = (uint32_t)syscall_dup3_impl(oldfd, newfd, flags);
        return;
    }

    if (syscall_no == SYSCALL_PIPE) {
        int* user_fds = (int*)sc_arg0(regs);
        sc_ret(regs) = (uint32_t)syscall_pipe_impl(user_fds);
        return;
    }

    if (syscall_no == SYSCALL_PIPE2) {
        int* user_fds = (int*)sc_arg0(regs);
        uint32_t flags = (uint32_t)sc_arg1(regs);
        sc_ret(regs) = (uint32_t)syscall_pipe2_impl(user_fds, flags);
        return;
    }

    if (syscall_no == SYSCALL_EXECVE) {
        const char* path = (const char*)sc_arg0(regs);
        const char* const* argv = (const char* const*)sc_arg1(regs);
        const char* const* envp = (const char* const*)sc_arg2(regs);
        sc_ret(regs) = (uint32_t)syscall_execve_impl(regs, path, argv, envp);
        return;
    }

    if (syscall_no == SYSCALL_FORK) {
        sc_ret(regs) = (uint32_t)syscall_fork_impl(regs);
        return;
    }

    if (syscall_no == SYSCALL_POSIX_SPAWN) {
        /* posix_spawn(pid_t* pid_out, path, argv, envp)
         * Combines fork+execve atomically.  Returns 0 on success and stores
         * child pid in *pid_out.  The child immediately execs path. */
        uint32_t* user_pid = (uint32_t*)sc_arg0(regs);
        const char* path    = (const char*)sc_arg1(regs);
        const char* const* argv = (const char* const*)sc_arg2(regs);
        const char* const* envp = (const char* const*)sc_arg3(regs);

        if (user_pid && user_range_ok(user_pid, 4) == 0) {
            sc_ret(regs) = (uint32_t)-EFAULT; return;
        }

        /* Fork: creates child with copy of parent's regs */
        int child_pid = syscall_fork_impl(regs);
        if (child_pid < 0) {
            sc_ret(regs) = (uint32_t)child_pid; return;
        }
        if (child_pid == 0) {
            /* We are in the child — exec immediately */
            int rc = syscall_execve_impl(regs, path, argv, envp);
            if (rc < 0) {
                /* execve failed — exit child */
                process_exit_notify(127);
                hal_cpu_enable_interrupts();
                schedule();
                for (;;) hal_cpu_idle();
            }
            return; /* execve rewrote regs, return to new program */
        }
        /* Parent: store child PID */
        if (user_pid) {
            uint32_t cpid = (uint32_t)child_pid;
            (void)copy_to_user(user_pid, &cpid, 4);
        }
        sc_ret(regs) = 0;
        return;
    }

    if (syscall_no == SYSCALL_POLL) {
        struct pollfd* fds = (struct pollfd*)sc_arg0(regs);
        uint32_t nfds = sc_arg1(regs);
        int32_t timeout = (int32_t)sc_arg2(regs);
        sc_ret(regs) = (uint32_t)syscall_poll_impl(fds, nfds, timeout);
        return;
    }

    if (syscall_no == SYSCALL_KILL) {
        uint32_t pid = sc_arg0(regs);
        int sig = (int)sc_arg1(regs);
        sc_ret(regs) = (uint32_t)process_kill(pid, sig);
        return;
    }

    if (syscall_no == SYSCALL_SIGQUEUE) {
        uint32_t pid = sc_arg0(regs);
        int sig = (int)sc_arg1(regs);
        /* arg2 = si_value (union sigval — int or pointer) — stored but
         * not yet delivered via siginfo because AdrOS uses a bitmask for
         * pending signals, not a queue.  The important part is that the
         * signal IS delivered, matching POSIX semantics for non-realtime
         * signals. */
        (void)sc_arg2(regs);
        sc_ret(regs) = (uint32_t)process_kill(pid, sig);
        return;
    }

    if (syscall_no == SYSCALL_SELECT) {
        uint32_t nfds = sc_arg0(regs);
        uint64_t* readfds = (uint64_t*)sc_arg1(regs);
        uint64_t* writefds = (uint64_t*)sc_arg2(regs);
        uint64_t* exceptfds = (uint64_t*)sc_arg3(regs);
        int32_t timeout = (int32_t)sc_arg4(regs);
        sc_ret(regs) = (uint32_t)syscall_select_impl(nfds, readfds, writefds, exceptfds, timeout);
        return;
    }

    if (syscall_no == SYSCALL_IOCTL) {
        int fd = (int)sc_arg0(regs);
        uint32_t cmd = (uint32_t)sc_arg1(regs);
        void* arg = (void*)sc_arg2(regs);
        sc_ret(regs) = (uint32_t)syscall_ioctl_impl(fd, cmd, arg);
        return;
    }

    if (syscall_no == SYSCALL_SETSID) {
        sc_ret(regs) = (uint32_t)syscall_setsid_impl();
        return;
    }

    if (syscall_no == SYSCALL_SETPGID) {
        int pid = (int)sc_arg0(regs);
        int pgid = (int)sc_arg1(regs);
        sc_ret(regs) = (uint32_t)syscall_setpgid_impl(pid, pgid);
        return;
    }

    if (syscall_no == SYSCALL_GETPGRP) {
        sc_ret(regs) = (uint32_t)syscall_getpgrp_impl();
        return;
    }

    if (syscall_no == SYSCALL_SIGACTION) {
        int sig = (int)sc_arg0(regs);
        const struct sigaction* act = (const struct sigaction*)sc_arg1(regs);
        struct sigaction* oldact = (struct sigaction*)sc_arg2(regs);
        sc_ret(regs) = (uint32_t)syscall_sigaction_impl(sig, act, oldact);
        return;
    }

    if (syscall_no == SYSCALL_SIGPROCMASK) {
        uint32_t how = sc_arg0(regs);
        uint32_t mask = sc_arg1(regs);
        uint32_t* old_out = (uint32_t*)sc_arg2(regs);
        sc_ret(regs) = (uint32_t)syscall_sigprocmask_impl(how, mask, old_out);
        return;
    }

    if (syscall_no == SYSCALL_SIGRETURN) {
        const void* user_frame = (const void*)(uintptr_t)sc_arg0(regs);
        sc_ret(regs) = (uint32_t)arch_sigreturn(regs, user_frame);
        return;
    }

    if (syscall_no == SYSCALL_FCNTL) {
        int fd = (int)sc_arg0(regs);
        int cmd = (int)sc_arg1(regs);
        uint32_t arg = (uint32_t)sc_arg2(regs);
        sc_ret(regs) = (uint32_t)syscall_fcntl_impl(fd, cmd, arg);
        return;
    }

    if (syscall_no == SYSCALL_MKDIR) {
        const char* path = (const char*)sc_arg0(regs);
        sc_ret(regs) = (uint32_t)syscall_mkdir_impl(path);
        return;
    }

    if (syscall_no == SYSCALL_UNLINK) {
        const char* path = (const char*)sc_arg0(regs);
        sc_ret(regs) = (uint32_t)syscall_unlink_impl(path);
        return;
    }

    if (syscall_no == SYSCALL_UNLINKAT) {
        int dirfd = (int)sc_arg0(regs);
        const char* path = (const char*)sc_arg1(regs);
        uint32_t flags = (uint32_t)sc_arg2(regs);
        sc_ret(regs) = (uint32_t)syscall_unlinkat_impl(dirfd, path, flags);
        return;
    }

    if (syscall_no == SYSCALL_GETDENTS) {
        int fd = (int)sc_arg0(regs);
        void* buf = (void*)sc_arg1(regs);
        uint32_t len = (uint32_t)sc_arg2(regs);
        sc_ret(regs) = (uint32_t)syscall_getdents_impl(fd, buf, len);
        return;
    }

    if (syscall_no == SYSCALL_RENAME) {
        const char* oldpath = (const char*)sc_arg0(regs);
        const char* newpath = (const char*)sc_arg1(regs);
        sc_ret(regs) = (uint32_t)syscall_rename_impl(oldpath, newpath);
        return;
    }

    if (syscall_no == SYSCALL_RMDIR) {
        const char* path = (const char*)sc_arg0(regs);
        sc_ret(regs) = (uint32_t)syscall_rmdir_impl(path);
        return;
    }

    if (syscall_no == SYSCALL_BRK) {
        uintptr_t addr = (uintptr_t)sc_arg0(regs);
        sc_ret(regs) = (uint32_t)syscall_brk_impl(addr);
        return;
    }

    if (syscall_no == SYSCALL_NANOSLEEP) {
        const struct timespec* req = (const struct timespec*)sc_arg0(regs);
        struct timespec* rem = (struct timespec*)sc_arg1(regs);
        sc_ret(regs) = (uint32_t)syscall_nanosleep_impl(req, rem);
        return;
    }

    if (syscall_no == SYSCALL_CLOCK_GETTIME) {
        uint32_t clk_id = sc_arg0(regs);
        struct timespec* tp = (struct timespec*)sc_arg1(regs);
        sc_ret(regs) = (uint32_t)syscall_clock_gettime_impl(clk_id, tp);
        return;
    }

    if (syscall_no == SYSCALL_MMAP) {
        uintptr_t addr = (uintptr_t)sc_arg0(regs);
        uint32_t length = sc_arg1(regs);
        uint32_t prot = sc_arg2(regs);
        uint32_t mflags = sc_arg3(regs);
        int fd = (int)sc_arg4(regs);
        sc_ret(regs) = (uint32_t)syscall_mmap_impl(addr, length, prot, mflags, fd, 0);
        return;
    }

    if (syscall_no == SYSCALL_MUNMAP) {
        uintptr_t addr = (uintptr_t)sc_arg0(regs);
        uint32_t length = sc_arg1(regs);
        sc_ret(regs) = (uint32_t)syscall_munmap_impl(addr, length);
        return;
    }

    if (syscall_no == SYSCALL_SHMGET) {
        uint32_t key = sc_arg0(regs);
        uint32_t size = sc_arg1(regs);
        int flags = (int)sc_arg2(regs);
        sc_ret(regs) = (uint32_t)shm_get(key, size, flags);
        return;
    }

    if (syscall_no == SYSCALL_SHMAT) {
        int shmid = (int)sc_arg0(regs);
        uintptr_t shmaddr = (uintptr_t)sc_arg1(regs);
        sc_ret(regs) = (uint32_t)(uintptr_t)shm_at(shmid, shmaddr);
        return;
    }

    if (syscall_no == SYSCALL_SHMDT) {
        const void* shmaddr = (const void*)sc_arg0(regs);
        sc_ret(regs) = (uint32_t)shm_dt(shmaddr);
        return;
    }

    if (syscall_no == SYSCALL_SHMCTL) {
        int shmid = (int)sc_arg0(regs);
        int cmd = (int)sc_arg1(regs);
        struct shmid_ds* buf = (struct shmid_ds*)sc_arg2(regs);
        sc_ret(regs) = (uint32_t)shm_ctl(shmid, cmd, buf);
        return;
    }

    if (syscall_no == SYSCALL_LINK) {
        const char* oldpath = (const char*)sc_arg0(regs);
        const char* newpath = (const char*)sc_arg1(regs);
        sc_ret(regs) = (uint32_t)syscall_link_impl(oldpath, newpath);
        return;
    }

    if (syscall_no == SYSCALL_SYMLINK) {
        const char* target = (const char*)sc_arg0(regs);
        const char* linkpath = (const char*)sc_arg1(regs);
        sc_ret(regs) = (uint32_t)syscall_symlink_impl(target, linkpath);
        return;
    }

    if (syscall_no == SYSCALL_READLINK) {
        const char* path = (const char*)sc_arg0(regs);
        char* buf = (char*)sc_arg1(regs);
        uint32_t bufsz = sc_arg2(regs);
        sc_ret(regs) = (uint32_t)syscall_readlink_impl(path, buf, bufsz);
        return;
    }

    if (syscall_no == SYSCALL_CHMOD) {
        const char* path = (const char*)sc_arg0(regs);
        uint32_t mode = sc_arg1(regs);
        sc_ret(regs) = (uint32_t)syscall_chmod_impl(path, mode);
        return;
    }

    if (syscall_no == SYSCALL_CHOWN) {
        const char* path = (const char*)sc_arg0(regs);
        uint32_t uid = sc_arg1(regs);
        uint32_t gid = sc_arg2(regs);
        sc_ret(regs) = (uint32_t)syscall_chown_impl(path, uid, gid);
        return;
    }

    if (syscall_no == SYSCALL_GETUID) {
        sc_ret(regs) = current_process ? current_process->uid : 0;
        return;
    }

    if (syscall_no == SYSCALL_GETGID) {
        sc_ret(regs) = current_process ? current_process->gid : 0;
        return;
    }

    if (syscall_no == SYSCALL_SET_THREAD_AREA) {
        uintptr_t base = (uintptr_t)sc_arg0(regs);
        if (!current_process) { sc_ret(regs) = (uint32_t)-EINVAL; return; }
        current_process->tls_base = base;
        hal_cpu_set_tls(base);
        sc_ret(regs) = 0;
        return;
    }

    if (syscall_no == SYSCALL_GETTID) {
        sc_ret(regs) = current_process ? current_process->pid : 0;
        return;
    }

    if (syscall_no == SYSCALL_CLONE) {
        sc_ret(regs) = (uint32_t)syscall_clone_impl(regs);
        return;
    }

    if (syscall_no == SYSCALL_SIGPENDING) {
        uint32_t* user_set = (uint32_t*)sc_arg0(regs);
        if (!current_process) { sc_ret(regs) = (uint32_t)-EINVAL; return; }
        uint32_t pending = current_process->sig_pending_mask & current_process->sig_blocked_mask;
        if (copy_to_user(user_set, &pending, sizeof(pending)) < 0) {
            sc_ret(regs) = (uint32_t)-EFAULT;
        } else {
            sc_ret(regs) = 0;
        }
        return;
    }

    if (syscall_no == SYSCALL_FSYNC || syscall_no == SYSCALL_FDATASYNC) {
        int fd = (int)sc_arg0(regs);
        if (!current_process || fd < 0 || fd >= PROCESS_MAX_FILES || !current_process->files[fd]) {
            sc_ret(regs) = (uint32_t)-EBADF;
        } else {
            sc_ret(regs) = 0;
        }
        return;
    }

    if (syscall_no == SYSCALL_PREAD || syscall_no == SYSCALL_PWRITE ||
        syscall_no == SYSCALL_ACCESS || syscall_no == SYSCALL_TRUNCATE ||
        syscall_no == SYSCALL_FTRUNCATE || syscall_no == SYSCALL_READV ||
        syscall_no == SYSCALL_WRITEV) {
        posix_ext_syscall_dispatch(regs, syscall_no);
        return;
    }

    if (syscall_no == SYSCALL_UMASK) {
        if (!current_process) { sc_ret(regs) = 0; return; }
        uint32_t old = current_process->umask;
        current_process->umask = sc_arg0(regs) & 0777;
        sc_ret(regs) = old;
        return;
    }

    if (syscall_no == SYSCALL_SETUID) {
        if (!current_process) { sc_ret(regs) = (uint32_t)-EINVAL; return; }
        uint32_t new_uid = sc_arg0(regs);
        if (current_process->euid == 0) {
            current_process->uid = new_uid;
            current_process->euid = new_uid;
        } else if (new_uid == current_process->uid) {
            current_process->euid = new_uid;
        } else {
            sc_ret(regs) = (uint32_t)-EPERM;
            return;
        }
        sc_ret(regs) = 0;
        return;
    }

    if (syscall_no == SYSCALL_SETGID) {
        if (!current_process) { sc_ret(regs) = (uint32_t)-EINVAL; return; }
        uint32_t new_gid = sc_arg0(regs);
        if (current_process->euid == 0) {
            current_process->gid = new_gid;
            current_process->egid = new_gid;
        } else if (new_gid == current_process->gid) {
            current_process->egid = new_gid;
        } else {
            sc_ret(regs) = (uint32_t)-EPERM;
            return;
        }
        sc_ret(regs) = 0;
        return;
    }

    if (syscall_no == SYSCALL_GETEUID) {
        sc_ret(regs) = current_process ? current_process->euid : 0;
        return;
    }

    if (syscall_no == SYSCALL_GETEGID) {
        sc_ret(regs) = current_process ? current_process->egid : 0;
        return;
    }

    if (syscall_no == SYSCALL_SETEUID) {
        if (!current_process) { sc_ret(regs) = (uint32_t)-EINVAL; return; }
        uint32_t new_euid = sc_arg0(regs);
        if (current_process->euid == 0 || new_euid == current_process->uid) {
            current_process->euid = new_euid;
            sc_ret(regs) = 0;
        } else {
            sc_ret(regs) = (uint32_t)-EPERM;
        }
        return;
    }

    if (syscall_no == SYSCALL_SETEGID) {
        if (!current_process) { sc_ret(regs) = (uint32_t)-EINVAL; return; }
        uint32_t new_egid = sc_arg0(regs);
        if (current_process->euid == 0 || new_egid == current_process->gid) {
            current_process->egid = new_egid;
            sc_ret(regs) = 0;
        } else {
            sc_ret(regs) = (uint32_t)-EPERM;
        }
        return;
    }

    if (syscall_no == SYSCALL_FLOCK) {
        int fd = (int)sc_arg0(regs);
        int operation = (int)sc_arg1(regs);
        if (!current_process || fd < 0 || fd >= PROCESS_MAX_FILES || !current_process->files[fd]) {
            sc_ret(regs) = (uint32_t)-EBADF;
        } else {
            uint32_t ino = current_process->files[fd]->node->inode;
            sc_ret(regs) = (uint32_t)flock_do(ino, current_process->pid, operation);
        }
        return;
    }

    if (syscall_no == SYSCALL_SIGALTSTACK ||
        syscall_no == SYSCALL_TIMES || syscall_no == SYSCALL_FUTEX) {
        posix_ext_syscall_dispatch(regs, syscall_no);
        return;
    }

    if (syscall_no == SYSCALL_ALARM) {
        if (!current_process) { sc_ret(regs) = 0; return; }
        uint32_t seconds = sc_arg0(regs);
        uint32_t now = get_tick_count();
        uint32_t new_tick = (seconds == 0) ? 0 : now + seconds * TICKS_PER_SEC;
        current_process->alarm_interval = 0; /* alarm() is always one-shot */
        uint32_t old_tick = process_alarm_set(current_process, new_tick);
        uint32_t old_remaining = 0;
        if (old_tick > now) {
            old_remaining = (old_tick - now) / TICKS_PER_SEC + 1;
        }
        sc_ret(regs) = old_remaining;
        return;
    }

    if (syscall_no == SYSCALL_SETITIMER) {
        if (!current_process) { sc_ret(regs) = (uint32_t)-EINVAL; return; }
        int which = (int)sc_arg0(regs);
        const void* user_new = (const void*)sc_arg1(regs);
        void* user_old = (void*)sc_arg2(regs);

        struct k_itimerval knew;
        memset(&knew, 0, sizeof(knew));
        if (user_new) {
            if (copy_from_user(&knew, user_new, sizeof(knew)) < 0) {
                sc_ret(regs) = (uint32_t)-EFAULT; return;
            }
        }

        uint32_t now = get_tick_count();

        if (which == ITIMER_REAL) {
            /* Return old value */
            if (user_old) {
                struct k_itimerval kold;
                memset(&kold, 0, sizeof(kold));
                ticks_to_timeval(current_process->alarm_interval, &kold.it_interval);
                if (current_process->alarm_tick > now)
                    ticks_to_timeval(current_process->alarm_tick - now, &kold.it_value);
                if (copy_to_user(user_old, &kold, sizeof(kold)) < 0) {
                    sc_ret(regs) = (uint32_t)-EFAULT; return;
                }
            }
            /* Set new value */
            uint32_t val_ticks = timeval_to_ticks(&knew.it_value);
            uint32_t int_ticks = timeval_to_ticks(&knew.it_interval);
            current_process->alarm_interval = int_ticks;
            process_alarm_set(current_process, val_ticks ? now + val_ticks : 0);
        } else if (which == ITIMER_VIRTUAL) {
            if (user_old) {
                struct k_itimerval kold;
                memset(&kold, 0, sizeof(kold));
                ticks_to_timeval(current_process->itimer_virt_interval, &kold.it_interval);
                ticks_to_timeval(current_process->itimer_virt_value, &kold.it_value);
                if (copy_to_user(user_old, &kold, sizeof(kold)) < 0) {
                    sc_ret(regs) = (uint32_t)-EFAULT; return;
                }
            }
            current_process->itimer_virt_value = timeval_to_ticks(&knew.it_value);
            current_process->itimer_virt_interval = timeval_to_ticks(&knew.it_interval);
        } else if (which == ITIMER_PROF) {
            if (user_old) {
                struct k_itimerval kold;
                memset(&kold, 0, sizeof(kold));
                ticks_to_timeval(current_process->itimer_prof_interval, &kold.it_interval);
                ticks_to_timeval(current_process->itimer_prof_value, &kold.it_value);
                if (copy_to_user(user_old, &kold, sizeof(kold)) < 0) {
                    sc_ret(regs) = (uint32_t)-EFAULT; return;
                }
            }
            current_process->itimer_prof_value = timeval_to_ticks(&knew.it_value);
            current_process->itimer_prof_interval = timeval_to_ticks(&knew.it_interval);
        } else {
            sc_ret(regs) = (uint32_t)-EINVAL; return;
        }
        sc_ret(regs) = 0;
        return;
    }

    if (syscall_no == SYSCALL_GETITIMER) {
        if (!current_process) { sc_ret(regs) = (uint32_t)-EINVAL; return; }
        int which = (int)sc_arg0(regs);
        void* user_val = (void*)sc_arg1(regs);
        if (!user_val) { sc_ret(regs) = (uint32_t)-EFAULT; return; }

        struct k_itimerval kval;
        memset(&kval, 0, sizeof(kval));
        uint32_t now = get_tick_count();

        if (which == ITIMER_REAL) {
            ticks_to_timeval(current_process->alarm_interval, &kval.it_interval);
            if (current_process->alarm_tick > now)
                ticks_to_timeval(current_process->alarm_tick - now, &kval.it_value);
        } else if (which == ITIMER_VIRTUAL) {
            ticks_to_timeval(current_process->itimer_virt_interval, &kval.it_interval);
            ticks_to_timeval(current_process->itimer_virt_value, &kval.it_value);
        } else if (which == ITIMER_PROF) {
            ticks_to_timeval(current_process->itimer_prof_interval, &kval.it_interval);
            ticks_to_timeval(current_process->itimer_prof_value, &kval.it_value);
        } else {
            sc_ret(regs) = (uint32_t)-EINVAL; return;
        }

        if (copy_to_user(user_val, &kval, sizeof(kval)) < 0) {
            sc_ret(regs) = (uint32_t)-EFAULT; return;
        }
        sc_ret(regs) = 0;
        return;
    }

    if (syscall_no == SYSCALL_SIGSUSPEND) {
        if (!current_process) { sc_ret(regs) = (uint32_t)-EINVAL; return; }
        uint32_t new_mask = 0;
        if (copy_from_user(&new_mask, (const void*)sc_arg0(regs), sizeof(new_mask)) < 0) {
            sc_ret(regs) = (uint32_t)-EFAULT; return;
        }
        uint32_t old_mask = current_process->sig_blocked_mask;
        current_process->sig_blocked_mask = new_mask;
        extern void schedule(void);
        while ((current_process->sig_pending_mask & ~current_process->sig_blocked_mask) == 0) {
            schedule();
        }
        current_process->sig_blocked_mask = old_mask;
        sc_ret(regs) = (uint32_t)-EINTR;
        return;
    }

    /* ---- Socket syscalls ---- */
    socket_syscall_dispatch(regs, syscall_no);
    /* If socket dispatch handled it, the return register is set and we return.
       If not, it sets ENOSYS. Either way, return. */
    return;
}

/* Separate function to keep pread/pwrite/access locals off syscall_handler's stack */
__attribute__((noinline))
static void posix_ext_syscall_dispatch(struct registers* regs, uint32_t syscall_no) {
    if (syscall_no == SYSCALL_PREAD) {
        int fd = (int)sc_arg0(regs);
        void* buf = (void*)sc_arg1(regs);
        uint32_t count = sc_arg2(regs);
        uint32_t offset = sc_arg3(regs);
        struct file* f = fd_get(fd);
        if (!f || !f->node) { sc_ret(regs) = (uint32_t)-EBADF; return; }
        if (!(f->node->f_ops && f->node->f_ops->read)) { sc_ret(regs) = (uint32_t)-ESPIPE; return; }
        if (count > 1024 * 1024) { sc_ret(regs) = (uint32_t)-EINVAL; return; }
        uint8_t kbuf[256];
        uint32_t total = 0;
        while (total < count) {
            uint32_t chunk = count - total;
            if (chunk > sizeof(kbuf)) chunk = (uint32_t)sizeof(kbuf);
            uint32_t rd = vfs_read(f->node, offset + total, chunk, kbuf);
            if (rd == 0) break;
            if (copy_to_user((uint8_t*)buf + total, kbuf, rd) < 0) {
                sc_ret(regs) = (uint32_t)-EFAULT; return;
            }
            total += rd;
            if (rd < chunk) break;
        }
        sc_ret(regs) = total;
        return;
    }

    if (syscall_no == SYSCALL_PWRITE) {
        int fd = (int)sc_arg0(regs);
        const void* buf = (const void*)sc_arg1(regs);
        uint32_t count = sc_arg2(regs);
        uint32_t offset = sc_arg3(regs);
        struct file* f = fd_get(fd);
        if (!f || !f->node) { sc_ret(regs) = (uint32_t)-EBADF; return; }
        if (!(f->node->f_ops && f->node->f_ops->write)) { sc_ret(regs) = (uint32_t)-ESPIPE; return; }
        if (count > 1024 * 1024) { sc_ret(regs) = (uint32_t)-EINVAL; return; }
        uint8_t kbuf[256];
        uint32_t total = 0;
        while (total < count) {
            uint32_t chunk = count - total;
            if (chunk > sizeof(kbuf)) chunk = (uint32_t)sizeof(kbuf);
            if (copy_from_user(kbuf, (const uint8_t*)buf + total, chunk) < 0) {
                sc_ret(regs) = (uint32_t)-EFAULT; return;
            }
            uint32_t wr = vfs_write(f->node, offset + total, chunk, kbuf);
            if (wr == 0) break;
            total += wr;
            if (wr < chunk) break;
        }
        sc_ret(regs) = total;
        return;
    }

    if (syscall_no == SYSCALL_ACCESS) {
        const char* user_path = (const char*)sc_arg0(regs);
        if (!user_path) { sc_ret(regs) = (uint32_t)-EFAULT; return; }
        char path[128];
        int prc = path_resolve_user(user_path, path, sizeof(path));
        if (prc < 0) { sc_ret(regs) = (uint32_t)prc; return; }
        fs_node_t* node = vfs_lookup(path);
        if (!node) { sc_ret(regs) = (uint32_t)-ENOENT; return; }
        sc_ret(regs) = 0;
        return;
    }

    if (syscall_no == SYSCALL_FTRUNCATE) {
        int fd = (int)sc_arg0(regs);
        uint32_t length = sc_arg1(regs);
        struct file* f = fd_get(fd);
        if (!f || !f->node) { sc_ret(regs) = (uint32_t)-EBADF; return; }
        if (!(f->node->flags & FS_FILE)) { sc_ret(regs) = (uint32_t)-EINVAL; return; }
        f->node->length = length;
        sc_ret(regs) = 0;
        return;
    }

    if (syscall_no == SYSCALL_TRUNCATE) {
        const char* user_path = (const char*)sc_arg0(regs);
        uint32_t length = sc_arg1(regs);
        if (!user_path) { sc_ret(regs) = (uint32_t)-EFAULT; return; }
        char path[128];
        int prc = path_resolve_user(user_path, path, sizeof(path));
        if (prc < 0) { sc_ret(regs) = (uint32_t)prc; return; }
        fs_node_t* node = vfs_lookup(path);
        if (!node) { sc_ret(regs) = (uint32_t)-ENOENT; return; }
        if (!(node->flags & FS_FILE)) { sc_ret(regs) = (uint32_t)-EISDIR; return; }
        node->length = length;
        sc_ret(regs) = 0;
        return;
    }

    if (syscall_no == SYSCALL_READV) {
        int fd = (int)sc_arg0(regs);
        struct { void* iov_base; uint32_t iov_len; } iov;
        const void* user_iov = (const void*)sc_arg1(regs);
        int iovcnt = (int)sc_arg2(regs);
        if (iovcnt <= 0 || iovcnt > 16) { sc_ret(regs) = (uint32_t)-EINVAL; return; }
        uint32_t total = 0;
        for (int i = 0; i < iovcnt; i++) {
            if (copy_from_user(&iov, (const char*)user_iov + i * 8, 8) < 0) {
                sc_ret(regs) = (uint32_t)-EFAULT; return;
            }
            if (iov.iov_len == 0) continue;
            int r = syscall_read_impl(fd, iov.iov_base, iov.iov_len);
            if (r < 0) { if (total == 0) { sc_ret(regs) = (uint32_t)r; return; } break; }
            total += (uint32_t)r;
            if ((uint32_t)r < iov.iov_len) break;
        }
        sc_ret(regs) = total;
        return;
    }

    if (syscall_no == SYSCALL_WRITEV) {
        int fd = (int)sc_arg0(regs);
        struct { const void* iov_base; uint32_t iov_len; } iov;
        const void* user_iov = (const void*)sc_arg1(regs);
        int iovcnt = (int)sc_arg2(regs);
        if (iovcnt <= 0 || iovcnt > 16) { sc_ret(regs) = (uint32_t)-EINVAL; return; }
        uint32_t total = 0;
        for (int i = 0; i < iovcnt; i++) {
            if (copy_from_user(&iov, (const char*)user_iov + i * 8, 8) < 0) {
                sc_ret(regs) = (uint32_t)-EFAULT; return;
            }
            if (iov.iov_len == 0) continue;
            int r = syscall_write_impl(fd, iov.iov_base, iov.iov_len);
            if (r < 0) { if (total == 0) { sc_ret(regs) = (uint32_t)r; return; } break; }
            total += (uint32_t)r;
            if ((uint32_t)r < iov.iov_len) break;
        }
        sc_ret(regs) = total;
        return;
    }

    if (syscall_no == SYSCALL_SIGALTSTACK) {
        if (!current_process) { sc_ret(regs) = (uint32_t)-EINVAL; return; }
        #ifndef SS_DISABLE
        #define SS_DISABLE 2
        #endif
        uint32_t* user_old = (uint32_t*)sc_arg1(regs);
        uint32_t* user_new = (uint32_t*)sc_arg0(regs);
        if (user_old) {
            uint32_t old_ss[3];
            old_ss[0] = (uint32_t)current_process->ss_sp;
            old_ss[1] = current_process->ss_flags;
            old_ss[2] = current_process->ss_size;
            if (copy_to_user(user_old, old_ss, 12) < 0) {
                sc_ret(regs) = (uint32_t)-EFAULT; return;
            }
        }
        if (user_new) {
            uint32_t new_ss[3];
            if (copy_from_user(new_ss, user_new, 12) < 0) {
                sc_ret(regs) = (uint32_t)-EFAULT; return;
            }
            if (new_ss[1] & SS_DISABLE) {
                current_process->ss_sp = 0;
                current_process->ss_size = 0;
                current_process->ss_flags = SS_DISABLE;
            } else {
                current_process->ss_sp = (uintptr_t)new_ss[0];
                current_process->ss_flags = new_ss[1];
                current_process->ss_size = new_ss[2];
            }
        }
        sc_ret(regs) = 0;
        return;
    }

    if (syscall_no == SYSCALL_FUTEX) {
        #define FUTEX_WAIT 0
        #define FUTEX_WAKE 1
        #define FUTEX_MAX_WAITERS 32
        static struct { uintptr_t addr; struct process* proc; } futex_waiters[FUTEX_MAX_WAITERS];
        
        uint32_t* uaddr = (uint32_t*)sc_arg0(regs);
        int op = (int)sc_arg1(regs);
        uint32_t val = sc_arg2(regs);
        
        if (!uaddr) { sc_ret(regs) = (uint32_t)-EFAULT; return; }
        
        if (op == FUTEX_WAIT) {
            uint32_t cur = 0;
            if (copy_from_user(&cur, uaddr, sizeof(cur)) < 0) {
                sc_ret(regs) = (uint32_t)-EFAULT; return;
            }
            if (cur != val) { sc_ret(regs) = (uint32_t)-EAGAIN; return; }
            /* Add to waiter list and sleep */
            int slot = -1;
            for (int i = 0; i < FUTEX_MAX_WAITERS; i++) {
                if (!futex_waiters[i].proc) { slot = i; break; }
            }
            if (slot < 0) { sc_ret(regs) = (uint32_t)-ENOMEM; return; }
            futex_waiters[slot].addr = (uintptr_t)uaddr;
            futex_waiters[slot].proc = current_process;
            extern void schedule(void);
            current_process->state = PROCESS_SLEEPING;
            current_process->wake_at_tick = get_tick_count() + 5000; /* 100s timeout */
            schedule();
            futex_waiters[slot].proc = 0;
            futex_waiters[slot].addr = 0;
            sc_ret(regs) = 0;
            return;
        }
        
        if (op == FUTEX_WAKE) {
            int woken = 0;
            int max_wake = (int)val;
            if (max_wake <= 0) max_wake = 1;
            for (int i = 0; i < FUTEX_MAX_WAITERS && woken < max_wake; i++) {
                if (futex_waiters[i].proc && futex_waiters[i].addr == (uintptr_t)uaddr) {
                    futex_waiters[i].proc->state = PROCESS_READY;
                    futex_waiters[i].proc = 0;
                    futex_waiters[i].addr = 0;
                    woken++;
                }
            }
            sc_ret(regs) = (uint32_t)woken;
            return;
        }
        
        sc_ret(regs) = (uint32_t)-ENOSYS;
        return;
    }

    if (syscall_no == SYSCALL_TIMES) {
        if (!current_process) { sc_ret(regs) = (uint32_t)-EINVAL; return; }
        struct { uint32_t tms_utime; uint32_t tms_stime; uint32_t tms_cutime; uint32_t tms_cstime; } tms;
        tms.tms_utime = current_process->utime;
        tms.tms_stime = current_process->stime;
        tms.tms_cutime = 0;
        tms.tms_cstime = 0;
        void* user_buf = (void*)sc_arg0(regs);
        if (user_buf) {
            if (copy_to_user(user_buf, &tms, sizeof(tms)) < 0) {
                sc_ret(regs) = (uint32_t)-EFAULT; return;
            }
        }
        sc_ret(regs) = get_tick_count();
        return;
    }

    sc_ret(regs) = (uint32_t)-ENOSYS;
}

/* --- Socket VFS node --- */

static uint32_t sock_node_read(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    (void)offset;
    if (!node || !buffer) return 0;
    int sid = (int)node->inode;
    int rc = ksocket_recv(sid, buffer, size, 0);
    return (rc > 0) ? (uint32_t)rc : 0;
}

static uint32_t sock_node_write(fs_node_t* node, uint32_t offset, uint32_t size, const uint8_t* buffer) {
    (void)offset;
    if (!node || !buffer) return 0;
    int sid = (int)node->inode;
    int rc = ksocket_send(sid, buffer, size, 0);
    return (rc > 0) ? (uint32_t)rc : 0;
}

static void sock_node_close(fs_node_t* node) {
    if (!node) return;
    ksocket_close((int)node->inode);
    kfree(node);
}

static int sock_node_poll(fs_node_t* node, int events) {
    if (!node) return VFS_POLL_ERR;
    return ksocket_poll((int)node->inode, events);
}

static const struct file_operations sock_fops = {
    .read  = sock_node_read,
    .write = sock_node_write,
    .close = sock_node_close,
    .poll  = sock_node_poll,
};

static fs_node_t* sock_node_create(int sid) {
    fs_node_t* n = (fs_node_t*)kmalloc(sizeof(fs_node_t));
    if (!n) return NULL;
    memset(n, 0, sizeof(*n));
    strcpy(n->name, "socket");
    n->flags = FS_SOCKET;
    n->inode = (uint32_t)sid;
    n->f_ops = &sock_fops;
    return n;
}

static inline int sock_fd_get_sid(int fd) {
    if (fd < 0 || fd >= PROCESS_MAX_FILES) return -EBADF;
    struct file* f = current_process->files[fd];
    if (!f || !f->node || f->node->flags != FS_SOCKET) return -EBADF;
    return (int)f->node->inode;
}

/* Separate function to keep socket locals off syscall_handler's stack frame */
__attribute__((noinline))
static void socket_syscall_dispatch(struct registers* regs, uint32_t syscall_no) {
    if (syscall_no == SYSCALL_SOCKET) {
        int domain   = (int)sc_arg0(regs);
        int type     = (int)sc_arg1(regs);
        int protocol = (int)sc_arg2(regs);
        int sid = ksocket_create(domain, type, protocol);
        if (sid < 0) { sc_ret(regs) = (uint32_t)sid; return; }
        fs_node_t* sn = sock_node_create(sid);
        if (!sn) { ksocket_close(sid); sc_ret(regs) = (uint32_t)-ENOMEM; return; }
        struct file* f = (struct file*)kmalloc(sizeof(struct file));
        if (!f) { sock_node_close(sn); sc_ret(regs) = (uint32_t)-ENOMEM; return; }
        f->node = sn;
        f->offset = 0;
        f->flags = 0;
        f->refcount = 1;
        int fd = fd_alloc(f);
        if (fd < 0) { sock_node_close(sn); kfree(f); sc_ret(regs) = (uint32_t)-EMFILE; return; }
        sc_ret(regs) = (uint32_t)fd;
        return;
    }

    if (syscall_no == SYSCALL_BIND) {
        int sid = sock_fd_get_sid((int)sc_arg0(regs));
        if (sid < 0) { sc_ret(regs) = (uint32_t)-EBADF; return; }
        struct sockaddr_in sa;
        if (copy_from_user(&sa, (const void*)sc_arg1(regs), sizeof(sa)) < 0) {
            sc_ret(regs) = (uint32_t)-EFAULT; return;
        }
        sc_ret(regs) = (uint32_t)ksocket_bind(sid, &sa);
        return;
    }

    if (syscall_no == SYSCALL_LISTEN) {
        int sid = sock_fd_get_sid((int)sc_arg0(regs));
        if (sid < 0) { sc_ret(regs) = (uint32_t)-EBADF; return; }
        sc_ret(regs) = (uint32_t)ksocket_listen(sid, (int)sc_arg1(regs));
        return;
    }

    if (syscall_no == SYSCALL_ACCEPT) {
        int sid = sock_fd_get_sid((int)sc_arg0(regs));
        if (sid < 0) { sc_ret(regs) = (uint32_t)-EBADF; return; }
        struct sockaddr_in sa;
        memset(&sa, 0, sizeof(sa));
        int new_sid = ksocket_accept(sid, &sa);
        if (new_sid < 0) { sc_ret(regs) = (uint32_t)new_sid; return; }
        fs_node_t* sn = sock_node_create(new_sid);
        if (!sn) { ksocket_close(new_sid); sc_ret(regs) = (uint32_t)-ENOMEM; return; }
        struct file* f = (struct file*)kmalloc(sizeof(struct file));
        if (!f) { sock_node_close(sn); sc_ret(regs) = (uint32_t)-ENOMEM; return; }
        f->node = sn;
        f->offset = 0;
        f->flags = 0;
        f->refcount = 1;
        int new_fd = fd_alloc(f);
        if (new_fd < 0) { sock_node_close(sn); kfree(f); sc_ret(regs) = (uint32_t)-EMFILE; return; }
        if (sc_arg1(regs)) {
            (void)copy_to_user((void*)sc_arg1(regs), &sa, sizeof(sa));
        }
        sc_ret(regs) = (uint32_t)new_fd;
        return;
    }

    if (syscall_no == SYSCALL_CONNECT) {
        int sid = sock_fd_get_sid((int)sc_arg0(regs));
        if (sid < 0) { sc_ret(regs) = (uint32_t)-EBADF; return; }
        struct sockaddr_in sa;
        if (copy_from_user(&sa, (const void*)sc_arg1(regs), sizeof(sa)) < 0) {
            sc_ret(regs) = (uint32_t)-EFAULT; return;
        }
        sc_ret(regs) = (uint32_t)ksocket_connect(sid, &sa);
        return;
    }

    if (syscall_no == SYSCALL_SEND) {
        int sid = sock_fd_get_sid((int)sc_arg0(regs));
        if (sid < 0) { sc_ret(regs) = (uint32_t)-EBADF; return; }
        size_t len = (size_t)sc_arg2(regs);
        if (!user_range_ok((const void*)sc_arg1(regs), len)) {
            sc_ret(regs) = (uint32_t)-EFAULT; return;
        }
        sc_ret(regs) = (uint32_t)ksocket_send(sid, (const void*)sc_arg1(regs), len, (int)sc_arg3(regs));
        return;
    }

    if (syscall_no == SYSCALL_RECV) {
        int sid = sock_fd_get_sid((int)sc_arg0(regs));
        if (sid < 0) { sc_ret(regs) = (uint32_t)-EBADF; return; }
        size_t len = (size_t)sc_arg2(regs);
        if (!user_range_ok((void*)sc_arg1(regs), len)) {
            sc_ret(regs) = (uint32_t)-EFAULT; return;
        }
        sc_ret(regs) = (uint32_t)ksocket_recv(sid, (void*)sc_arg1(regs), len, (int)sc_arg3(regs));
        return;
    }

    if (syscall_no == SYSCALL_SENDTO) {
        int sid = sock_fd_get_sid((int)sc_arg0(regs));
        if (sid < 0) { sc_ret(regs) = (uint32_t)-EBADF; return; }
        size_t len = (size_t)sc_arg2(regs);
        if (!user_range_ok((const void*)sc_arg1(regs), len)) {
            sc_ret(regs) = (uint32_t)-EFAULT; return;
        }
        struct sockaddr_in dest;
        if (copy_from_user(&dest, (const void*)sc_arg4(regs), sizeof(dest)) < 0) {
            sc_ret(regs) = (uint32_t)-EFAULT; return;
        }
        sc_ret(regs) = (uint32_t)ksocket_sendto(sid, (const void*)sc_arg1(regs), len,
                                              (int)sc_arg3(regs), &dest);
        return;
    }

    if (syscall_no == SYSCALL_RECVFROM) {
        int sid = sock_fd_get_sid((int)sc_arg0(regs));
        if (sid < 0) { sc_ret(regs) = (uint32_t)-EBADF; return; }
        size_t len = (size_t)sc_arg2(regs);
        if (!user_range_ok((void*)sc_arg1(regs), len)) {
            sc_ret(regs) = (uint32_t)-EFAULT; return;
        }
        struct sockaddr_in src;
        memset(&src, 0, sizeof(src));
        int ret = ksocket_recvfrom(sid, (void*)sc_arg1(regs), len, (int)sc_arg3(regs), &src);
        if (ret > 0 && sc_arg4(regs)) {
            (void)copy_to_user((void*)sc_arg4(regs), &src, sizeof(src));
        }
        sc_ret(regs) = (uint32_t)ret;
        return;
    }

    if (syscall_no == SYSCALL_SETITIMER) {
        /* setitimer(which, user_new_value, user_old_value)
         * struct itimerval { uint32_t it_interval; uint32_t it_value; } (ticks) */
        uint32_t which = sc_arg0(regs);
        void* user_new = (void*)sc_arg1(regs);
        void* user_old = (void*)sc_arg2(regs);
        if (!current_process) { sc_ret(regs) = (uint32_t)-EINVAL; return; }

        uint32_t pair[2]; /* [0]=it_interval, [1]=it_value */

        if (user_old) {
            if (user_range_ok(user_old, 8) == 0) { sc_ret(regs) = (uint32_t)-EFAULT; return; }
            uint32_t old[2] = {0, 0};
            if (which == 0) { /* ITIMER_REAL */
                old[0] = current_process->alarm_interval;
                extern uint32_t get_tick_count(void);
                uint32_t now = get_tick_count();
                old[1] = (current_process->alarm_tick > now) ? current_process->alarm_tick - now : 0;
            } else if (which == 1) { /* ITIMER_VIRTUAL */
                old[0] = current_process->itimer_virt_interval;
                old[1] = current_process->itimer_virt_value;
            } else if (which == 2) { /* ITIMER_PROF */
                old[0] = current_process->itimer_prof_interval;
                old[1] = current_process->itimer_prof_value;
            }
            if (copy_to_user(user_old, old, 8) < 0) { sc_ret(regs) = (uint32_t)-EFAULT; return; }
        }

        if (user_new) {
            if (user_range_ok(user_new, 8) == 0) { sc_ret(regs) = (uint32_t)-EFAULT; return; }
            if (copy_from_user(pair, user_new, 8) < 0) { sc_ret(regs) = (uint32_t)-EFAULT; return; }
        } else {
            pair[0] = 0; pair[1] = 0;
        }

        if (which == 0) { /* ITIMER_REAL — uses alarm queue */
            current_process->alarm_interval = pair[0];
            if (pair[1] > 0) {
                extern uint32_t get_tick_count(void);
                process_alarm_set(current_process, get_tick_count() + pair[1]);
            } else {
                process_alarm_set(current_process, 0);
            }
        } else if (which == 1) { /* ITIMER_VIRTUAL */
            current_process->itimer_virt_interval = pair[0];
            current_process->itimer_virt_value = pair[1];
        } else if (which == 2) { /* ITIMER_PROF */
            current_process->itimer_prof_interval = pair[0];
            current_process->itimer_prof_value = pair[1];
        } else {
            sc_ret(regs) = (uint32_t)-EINVAL; return;
        }
        sc_ret(regs) = 0;
        return;
    }

    if (syscall_no == SYSCALL_GETITIMER) {
        uint32_t which = sc_arg0(regs);
        void* user_val = (void*)sc_arg1(regs);
        if (!current_process) { sc_ret(regs) = (uint32_t)-EINVAL; return; }
        if (!user_val || user_range_ok(user_val, 8) == 0) { sc_ret(regs) = (uint32_t)-EFAULT; return; }

        uint32_t out[2] = {0, 0};
        if (which == 0) {
            out[0] = current_process->alarm_interval;
            extern uint32_t get_tick_count(void);
            uint32_t now = get_tick_count();
            out[1] = (current_process->alarm_tick > now) ? current_process->alarm_tick - now : 0;
        } else if (which == 1) {
            out[0] = current_process->itimer_virt_interval;
            out[1] = current_process->itimer_virt_value;
        } else if (which == 2) {
            out[0] = current_process->itimer_prof_interval;
            out[1] = current_process->itimer_prof_value;
        } else {
            sc_ret(regs) = (uint32_t)-EINVAL; return;
        }
        if (copy_to_user(user_val, out, 8) < 0) { sc_ret(regs) = (uint32_t)-EFAULT; return; }
        sc_ret(regs) = 0;
        return;
    }

    if (syscall_no == SYSCALL_MQ_OPEN) {
        const char* name = (const char*)sc_arg0(regs);
        uint32_t oflag = sc_arg1(regs);
        sc_ret(regs) = (uint32_t)syscall_mq_open_impl(name, oflag);
        return;
    }
    if (syscall_no == SYSCALL_MQ_CLOSE) {
        sc_ret(regs) = (uint32_t)syscall_mq_close_impl((int)sc_arg0(regs));
        return;
    }
    if (syscall_no == SYSCALL_MQ_SEND) {
        sc_ret(regs) = (uint32_t)syscall_mq_send_impl(
            (int)sc_arg0(regs), (const void*)sc_arg1(regs),
            sc_arg2(regs), sc_arg3(regs));
        return;
    }
    if (syscall_no == SYSCALL_MQ_RECEIVE) {
        sc_ret(regs) = (uint32_t)syscall_mq_receive_impl(
            (int)sc_arg0(regs), (void*)sc_arg1(regs),
            sc_arg2(regs), (uint32_t*)sc_arg3(regs));
        return;
    }
    if (syscall_no == SYSCALL_MQ_UNLINK) {
        sc_ret(regs) = (uint32_t)syscall_mq_unlink_impl((const char*)sc_arg0(regs));
        return;
    }

    if (syscall_no == SYSCALL_SEM_OPEN) {
        sc_ret(regs) = (uint32_t)syscall_sem_open_impl(
            (const char*)sc_arg0(regs), sc_arg1(regs), sc_arg2(regs));
        return;
    }
    if (syscall_no == SYSCALL_SEM_CLOSE) {
        sc_ret(regs) = (uint32_t)syscall_sem_close_impl((int)sc_arg0(regs));
        return;
    }
    if (syscall_no == SYSCALL_SEM_WAIT) {
        sc_ret(regs) = (uint32_t)syscall_sem_wait_impl((int)sc_arg0(regs));
        return;
    }
    if (syscall_no == SYSCALL_SEM_POST) {
        sc_ret(regs) = (uint32_t)syscall_sem_post_impl((int)sc_arg0(regs));
        return;
    }
    if (syscall_no == SYSCALL_SEM_UNLINK) {
        sc_ret(regs) = (uint32_t)syscall_sem_unlink_impl((const char*)sc_arg0(regs));
        return;
    }
    if (syscall_no == SYSCALL_SEM_GETVALUE) {
        sc_ret(regs) = (uint32_t)syscall_sem_getvalue_impl(
            (int)sc_arg0(regs), (int*)sc_arg1(regs));
        return;
    }

    if (syscall_no == SYSCALL_GETADDRINFO) {
        /* getaddrinfo(user_hostname, user_out_ip)
         * Resolves hostname to IPv4 address (network byte order).
         * Checks built-in hosts table first, then falls back to DNS. */
        const char* user_host = (const char*)sc_arg0(regs);
        uint32_t* user_out = (uint32_t*)sc_arg1(regs);
        if (!user_host || !user_out) { sc_ret(regs) = (uint32_t)-EFAULT; return; }
        if (user_range_ok(user_out, 4) == 0) { sc_ret(regs) = (uint32_t)-EFAULT; return; }

        char host[128];
        if (copy_from_user(host, user_host, 127) < 0) { sc_ret(regs) = (uint32_t)-EFAULT; return; }
        host[127] = 0;

        /* Built-in /etc/hosts equivalent */
        uint32_t ip = 0;
        if (strcmp(host, "localhost") == 0 || strcmp(host, "localhost.localdomain") == 0) {
            ip = 0x0100007FU; /* 127.0.0.1 in network byte order (little-endian) */
        }

        if (ip == 0) {
            /* Try kernel DNS resolver */
            extern int dns_resolve(const char* hostname, uint32_t* out_ip);
            int rc = dns_resolve(host, &ip);
            if (rc < 0) { sc_ret(regs) = (uint32_t)-ENOENT; return; }
        }

        if (copy_to_user(user_out, &ip, 4) < 0) { sc_ret(regs) = (uint32_t)-EFAULT; return; }
        sc_ret(regs) = 0;
        return;
    }

    if (syscall_no == SYSCALL_DLOPEN) {
        sc_ret(regs) = (uint32_t)syscall_dlopen_impl((const char*)sc_arg0(regs));
        return;
    }
    if (syscall_no == SYSCALL_DLSYM) {
        sc_ret(regs) = (uint32_t)syscall_dlsym_impl(
            (int)sc_arg0(regs), (const char*)sc_arg1(regs), (uint32_t*)sc_arg2(regs));
        return;
    }
    if (syscall_no == SYSCALL_DLCLOSE) {
        sc_ret(regs) = (uint32_t)syscall_dlclose_impl((int)sc_arg0(regs));
        return;
    }

    sc_ret(regs) = (uint32_t)-ENOSYS;
}

void syscall_init(void) {
    arch_syscall_init();
}
