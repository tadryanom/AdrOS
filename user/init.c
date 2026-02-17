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
            execve(argv[0], (const char* const*)argv, NULL);
            /* If execve fails, try with /bin/sh -c */
            const char* sh_argv[] = { "/bin/sh", "-c", cmd, NULL };
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
static void default_init(void) {
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
            const char* argv[] = { "/bin/sh", NULL };
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

    printf("AdrOS init starting (PID %d)\n", getpid());

    /* Try to parse inittab */
    if (parse_inittab() < 0) {
        printf("init: no /etc/inittab, using defaults\n");
        default_init();
        return 0;  /* unreachable */
    }

    printf("init: loaded %d inittab entries, runlevel %d\n",
           nentries, current_runlevel);

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
