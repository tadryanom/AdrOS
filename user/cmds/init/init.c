// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2018, Tulio A M Mendes <tadryanom@hotmail.com>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://github.com/tadryanom/AdrOS
 */

/* AdrOS SysV-like init (/sbin/init)
 *
 * Reads /etc/inittab for configuration.
 * Supports runlevels 0-6 and S (single-user).
 * Actions: sysinit, respawn, wait, once, ctrlaltdel, shutdown.
 *
 * Default behavior (no inittab):
 *   1. Run /etc/init.d/rcS (if exists)
 *   2. Spawn /bin/sh on /dev/console
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <sys/mount.h>

#define MAX_ENTRIES 32
#define LINE_MAX   256

/* Inittab entry actions */
enum action {
    ACT_SYSINIT,    /* Run during system initialization */
    ACT_WAIT,       /* Run and wait for completion */
    ACT_ONCE,       /* Run once when entering runlevel */
    ACT_RESPAWN,    /* Restart when process dies */
    ACT_CTRLALTDEL, /* Run on Ctrl+Alt+Del */
    ACT_SHUTDOWN,   /* Run during shutdown */
};

struct inittab_entry {
    char id[8];
    char runlevels[16];
    enum action action;
    char process[128];
    int  pid;           /* PID of running process (for respawn) */
    int  active;
};

static struct inittab_entry entries[MAX_ENTRIES];
static int nentries = 0;
static int current_runlevel = 3;  /* Default: multi-user */
static volatile int do_shutdown = 0;   /* 1=poweroff, 2=reboot */

static void sigusr1_handler(int sig) { (void)sig; do_shutdown = 1; }
static void sigusr2_handler(int sig) { (void)sig; do_shutdown = 2; }

static enum action parse_action(const char* s) {
    if (strcmp(s, "sysinit") == 0) return ACT_SYSINIT;
    if (strcmp(s, "wait") == 0) return ACT_WAIT;
    if (strcmp(s, "once") == 0) return ACT_ONCE;
    if (strcmp(s, "respawn") == 0) return ACT_RESPAWN;
    if (strcmp(s, "ctrlaltdel") == 0) return ACT_CTRLALTDEL;
    if (strcmp(s, "shutdown") == 0) return ACT_SHUTDOWN;
    return ACT_ONCE;
}

/* Parse /etc/inittab
 * Format: id:runlevels:action:process
 * Example:
 *   ::sysinit:/etc/init.d/rcS
 *   ::respawn:/bin/sh
 *   tty1:2345:respawn:/bin/sh
 */
static int parse_inittab(void) {
    int fd = open("/etc/inittab", O_RDONLY);
    if (fd < 0) return -1;

    char buf[2048];
    int total = 0;
    int r;
    while ((r = read(fd, buf + total, (size_t)(sizeof(buf) - (size_t)total - 1))) > 0)
        total += r;
    buf[total] = '\0';
    close(fd);

    if (total >= (int)sizeof(buf) - 1) {
        write(2, "init: warning: /etc/inittab truncated\n", 38);
    }

    char* p = buf;
    while (*p && nentries < MAX_ENTRIES) {
        /* Skip whitespace and comments */
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\n') {
            while (*p && *p != '\n') p++;
            if (*p == '\n') p++;
            continue;
        }
        if (*p == '\0') break;

        struct inittab_entry* e = &entries[nentries];
        memset(e, 0, sizeof(*e));

        /* id */
        char* start = p;
        while (*p && *p != ':') p++;
        int len = (int)(p - start);
        if (len > 7) len = 7;
        memcpy(e->id, start, (size_t)len);
        e->id[len] = '\0';
        if (*p == ':') p++;

        /* runlevels */
        start = p;
        while (*p && *p != ':') p++;
        len = (int)(p - start);
        if (len > 15) len = 15;
        memcpy(e->runlevels, start, (size_t)len);
        e->runlevels[len] = '\0';
        if (*p == ':') p++;

        /* action */
        start = p;
        while (*p && *p != ':') p++;
        char action_str[32];
        len = (int)(p - start);
        if (len > 31) len = 31;
        memcpy(action_str, start, (size_t)len);
        action_str[len] = '\0';
        e->action = parse_action(action_str);
        if (*p == ':') p++;

        /* process */
        start = p;
        while (*p && *p != '\n') p++;
        len = (int)(p - start);
        if (len > 127) len = 127;
        memcpy(e->process, start, (size_t)len);
        e->process[len] = '\0';
        if (*p == '\n') p++;

        e->pid = -1;
        e->active = 1;
        nentries++;
    }

    return nentries > 0 ? 0 : -1;
}

/* Check if entry should run at current runlevel */
static int entry_matches_runlevel(const struct inittab_entry* e) {
    if (e->runlevels[0] == '\0') return 1;  /* Empty = all runlevels */
    char rl = '0' + (char)current_runlevel;
    for (const char* p = e->runlevels; *p; p++) {
        if (*p == rl || *p == 'S' || *p == 's') return 1;
    }
    return 0;
}

/* Run a process (fork + exec) */
static int run_process(const char* cmd) {
    int pid = fork();
    if (pid < 0) return -1;

    if (pid == 0) {
        /* Child: parse command into argv */
        char buf[128];
        strncpy(buf, cmd, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';

        char* argv[16];
        int argc = 0;
        char* p = buf;
        while (*p && argc < 15) {
            while (*p == ' ' || *p == '\t') p++;
            if (*p == '\0') break;
            argv[argc++] = p;
            while (*p && *p != ' ' && *p != '\t') p++;
            if (*p) *p++ = '\0';
        }
        argv[argc] = NULL;

        if (argc > 0) {
            execve(argv[0], argv, NULL);
            /* If execve fails, try with /bin/sh -c */
            char* sh_argv[] = { "/bin/sh", "-c", (char*)cmd, NULL };
            execve("/bin/sh", sh_argv, NULL);
        }
        _exit(127);
    }

    return pid;
}

/* Run a process and wait for it */
static int run_and_wait(const char* cmd) {
    int pid = run_process(cmd);
    if (pid < 0) return -1;
    int st;
    waitpid(pid, &st, 0);
    return st;
}

/* Run all entries matching a given action and current runlevel */
static void run_action(enum action act, int do_wait) {
    for (int i = 0; i < nentries; i++) {
        if (entries[i].action != act) continue;
        if (!entry_matches_runlevel(&entries[i])) continue;
        if (!entries[i].active) continue;

        if (do_wait) {
            run_and_wait(entries[i].process);
        } else {
            entries[i].pid = run_process(entries[i].process);
        }
    }
}

/* Respawn dead children */
static void check_respawn(void) {
    for (int i = 0; i < nentries; i++) {
        if (entries[i].action != ACT_RESPAWN) continue;
        if (!entry_matches_runlevel(&entries[i])) continue;
        if (!entries[i].active) continue;

        if (entries[i].pid <= 0) {
            entries[i].pid = run_process(entries[i].process);
        } else {
            /* Check if still running */
            int st;
            int r = waitpid(entries[i].pid, &st, 1 /* WNOHANG */);
            if (r > 0) {
                /* Process exited, respawn */
                entries[i].pid = run_process(entries[i].process);
            }
        }
    }
}

/* Default behavior when no inittab exists */
/* Mount virtual filesystems (migrated from kernel-space).
 * These must be done before spawning the shell since /dev/console
 * is needed for terminal I/O. */
static int is_mounted(const char* mountpoint) {
    /* Check /proc/mounts for an existing entry */
    int fd = open("/proc/mounts", 0);
    if (fd < 0) return 0;
    char buf[2048];
    int n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) return 0;
    buf[n] = '\0';

    int mplen = 0;
    const char* p = mountpoint;
    while (*p++) mplen++;

    /* Scan lines: source mountpoint fstype options ... */
    char* line = buf;
    while (line && *line) {
        char* nl = line;
        while (*nl && *nl != '\n') nl++;
        int eol = (*nl == '\n');
        if (eol) *nl = '\0';
        /* Skip source */
        char* mp = line;
        while (*mp && *mp != ' ') mp++;
        if (*mp) mp++;
        /* Compare mountpoint */
        const char* lmp = mp;
        const char* pmp = mountpoint;
        while (*lmp != ' ' && *lmp != '\0' && *pmp && *lmp == *pmp) { lmp++; pmp++; }
        if (*lmp == ' ' && *pmp == '\0') return 1;
        line = eol ? nl + 1 : nl;
    }
    return 0;
}

static void mount_virtual_fs(void) {
    /* Only mount if not already mounted by kernel init.
     * vfs_mount replaces existing entries, but re-creating root nodes
     * (e.g. tmpfs) leaks the old instance. */
    if (!is_mounted("/dev"))
        if (mount("none", "/dev", "devfs", 0, NULL) < 0)
            fprintf(stderr, "init: mount devfs on /dev failed\n");
    if (!is_mounted("/proc"))
        if (mount("none", "/proc", "procfs", 0, NULL) < 0)
            fprintf(stderr, "init: mount procfs on /proc failed\n");
    if (!is_mounted("/tmp"))
        if (mount("none", "/tmp", "tmpfs", 0, NULL) < 0)
            fprintf(stderr, "init: mount tmpfs on /tmp failed\n");
}

/* Parse /etc/fstab and mount disk-based filesystems.
 * Format: device mountpoint fstype options
 * Example: /dev/hda /disk ext2 defaults
 * Migrated from kernel init_parse_fstab() to userspace. */
static void parse_fstab(void) {
    int fd = open("/etc/fstab", O_RDONLY);
    if (fd < 0) return;

    char buf[2048];
    int total = 0, r;
    while ((r = read(fd, buf + total, (size_t)(sizeof(buf) - (size_t)total - 1))) > 0)
        total += r;
    buf[total] = '\0';
    close(fd);

    char* p = buf;
    while (*p) {
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0') break;
        if (*p == '#' || *p == '\n') {
            while (*p && *p != '\n') p++;
            if (*p == '\n') p++;
            continue;
        }

        /* device */
        char device[64] = {0};
        { char* s = p; while (*p && *p != ' ' && *p != '\t' && *p != '\n') p++;
          int len = (int)(p - s); if (len > 63) len = 63; memcpy(device, s, (size_t)len); }
        if (*p == '\n' || *p == '\0') { if (*p == '\n') p++; continue; }
        while (*p == ' ' || *p == '\t') p++;

        /* mountpoint */
        char mountpoint[64] = {0};
        { char* s = p; while (*p && *p != ' ' && *p != '\t' && *p != '\n') p++;
          int len = (int)(p - s); if (len > 63) len = 63; memcpy(mountpoint, s, (size_t)len); }
        if (*p == '\n' || *p == '\0') { if (*p == '\n') p++; continue; }
        while (*p == ' ' || *p == '\t') p++;

        /* fstype */
        char fstype[32] = {0};
        { char* s = p; while (*p && *p != ' ' && *p != '\t' && *p != '\n') p++;
          int len = (int)(p - s); if (len > 31) len = 31; memcpy(fstype, s, (size_t)len); }
        while (*p == ' ' || *p == '\t') p++;

        /* options (comma-separated: ro,rw,nosuid,nodev,noexec,defaults) */
        char options[128] = {0};
        { char* s = p; while (*p && *p != ' ' && *p != '\t' && *p != '\n') p++;
          int len = (int)(p - s); if (len > 127) len = 127; memcpy(options, s, (size_t)len); }

        /* Skip rest of line */
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;

        /* Skip virtual FS — already mounted by mount_virtual_fs() */
        if (strcmp(fstype, "devfs") == 0 || strcmp(fstype, "procfs") == 0
            || strcmp(fstype, "tmpfs") == 0 || strcmp(fstype, "overlayfs") == 0)
            continue;

        /* Skip if already mounted */
        if (is_mounted(mountpoint)) continue;

        /* Parse options into mount flags */
        unsigned long mflags = 0;
        if (options[0] != '\0' && strcmp(options, "defaults") != 0) {
            char optcopy[128];
            strncpy(optcopy, options, sizeof(optcopy) - 1);
            optcopy[sizeof(optcopy) - 1] = '\0';
            char* tok = optcopy;
            while (*tok) {
                char* comma = tok;
                while (*comma && *comma != ',') comma++;
                int is_last = (*comma == '\0');
                if (*comma == ',') *comma = '\0';
                if (strcmp(tok, "ro") == 0)          mflags |= MS_RDONLY;
                else if (strcmp(tok, "nosuid") == 0) mflags |= MS_NOSUID;
                else if (strcmp(tok, "nodev") == 0)  mflags |= MS_NODEV;
                else if (strcmp(tok, "noexec") == 0)  mflags |= MS_NOEXEC;
                if (is_last) break;
                tok = comma + 1;
            }
        }

        if (mount(device, mountpoint, fstype, mflags, NULL) < 0) {
            fprintf(stderr, "init: mount %s on %s (%s) failed\n",
                    device, mountpoint, fstype);
        } else {
            printf("init: mounted %s on %s (%s)\n", device, mountpoint, fstype);
        }
    }
}

static void default_init(void) {
    /* Mount virtual filesystems before anything else */
    mount_virtual_fs();

    /* Mount disk filesystems from /etc/fstab */
    parse_fstab();

    /* Run /etc/init.d/rcS if it exists */
    if (access("/etc/init.d/rcS", 0) == 0) {
        run_and_wait("/etc/init.d/rcS");
    }

    /* Spawn shell */
    while (1) {
        int pid = fork();
        if (pid < 0) {
            fprintf(stderr, "init: fork failed\n");
            for (;;) { struct timespec ts = {1,0}; nanosleep(&ts, NULL); }
        }

        if (pid == 0) {
            char* argv[] = { "/bin/sh", NULL };
            execve("/bin/sh", argv, NULL);
            _exit(127);
        }

        int st;
        waitpid(pid, &st, 0);

        /* Shell exited, respawn after a small delay */
        struct timespec ts = {1, 0};
        nanosleep(&ts, NULL);
    }
}

int main(int argc, char** argv) {
    (void)argc; (void)argv;

    /* PID 1 should not die on signals */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = (uintptr_t)SIG_IGN;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    /* SIGUSR1 = poweroff, SIGUSR2 = reboot */
    sa.sa_handler = (uintptr_t)sigusr1_handler;
    sigaction(SIGUSR1, &sa, NULL);
    sa.sa_handler = (uintptr_t)sigusr2_handler;
    sigaction(SIGUSR2, &sa, NULL);

    printf("AdrOS init starting (PID %d)\n", getpid());

    /* Try to parse inittab */
    if (parse_inittab() < 0) {
        printf("init: no /etc/inittab, using defaults\n");
        default_init();
        return 0;  /* unreachable */
    }

    printf("init: loaded %d inittab entries, runlevel %d\n",
           nentries, current_runlevel);

    /* Mount virtual filesystems before running any inittab entries */
    mount_virtual_fs();

    /* Mount disk filesystems from /etc/fstab */
    parse_fstab();

    /* Phase 1: sysinit entries */
    run_action(ACT_SYSINIT, 1);

    /* Phase 2: wait entries */
    run_action(ACT_WAIT, 1);

    /* Phase 3: once entries */
    run_action(ACT_ONCE, 0);

    /* Phase 4: respawn entries */
    run_action(ACT_RESPAWN, 0);

    /* Main loop: reap children and respawn */
    while (1) {
        if (do_shutdown) {
            printf("init: shutting down (%s)\n",
                   do_shutdown == 1 ? "poweroff" : "reboot");
            run_action(ACT_SHUTDOWN, 1);
            /* Kill all processes */
            kill(-1, SIGTERM);
            struct timespec ts = {2, 0};
            nanosleep(&ts, NULL);
            kill(-1, SIGKILL);
            if (do_shutdown == 1) {
                printf("init: System halted\n");
                _exit(0);
            } else {
                printf("init: Rebooting...\n");
                /* Attempt reboot via triple-fork or just exit */
                _exit(0);
            }
        }

        int st;
        int pid = waitpid(-1, &st, 0);

        if (pid > 0) {
            /* Mark dead child and respawn if needed */
            for (int i = 0; i < nentries; i++) {
                if (entries[i].pid == pid) {
                    entries[i].pid = -1;
                    break;
                }
            }
            check_respawn();
        } else {
            /* No children or error — sleep briefly */
            struct timespec ts = {1, 0};
            nanosleep(&ts, NULL);
            check_respawn();
        }
    }

    return 0;
}
