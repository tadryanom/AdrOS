/* AdrOS minimal POSIX sh */
#include <stdint.h>
#include "user_errno.h"

enum {
    SYSCALL_WRITE   = 1,
    SYSCALL_EXIT    = 2,
    SYSCALL_GETPID  = 3,
    SYSCALL_OPEN    = 4,
    SYSCALL_READ    = 5,
    SYSCALL_CLOSE   = 6,
    SYSCALL_WAITPID = 7,
    SYSCALL_DUP2    = 13,
    SYSCALL_PIPE    = 14,
    SYSCALL_EXECVE  = 15,
    SYSCALL_FORK    = 16,
};

enum { O_RDONLY = 0, O_WRONLY = 1, O_CREAT = 0x40, O_TRUNC = 0x200 };

/* ---- syscall wrappers ---- */

static int sys_write(int fd, const void* buf, uint32_t len) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret)
        : "a"(SYSCALL_WRITE), "b"(fd), "c"(buf), "d"(len) : "memory");
    return __syscall_fix(ret);
}

static int sys_read(int fd, void* buf, uint32_t len) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret)
        : "a"(SYSCALL_READ), "b"(fd), "c"(buf), "d"(len) : "memory");
    return __syscall_fix(ret);
}

static int sys_open(const char* path, int flags) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret)
        : "a"(SYSCALL_OPEN), "b"(path), "c"(flags) : "memory");
    return __syscall_fix(ret);
}

static int sys_close(int fd) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret)
        : "a"(SYSCALL_CLOSE), "b"(fd) : "memory");
    return __syscall_fix(ret);
}

static int sys_fork(void) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret)
        : "a"(SYSCALL_FORK) : "memory");
    return __syscall_fix(ret);
}

static int sys_execve(const char* p, char* const* av,
                      char* const* ev) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret)
        : "a"(SYSCALL_EXECVE), "b"(p), "c"(av), "d"(ev) : "memory");
    return __syscall_fix(ret);
}

static int sys_waitpid(int pid, int* status, int opts) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret)
        : "a"(SYSCALL_WAITPID), "b"(pid), "c"(status), "d"(opts) : "memory");
    return __syscall_fix(ret);
}

static int sys_dup2(int oldfd, int newfd) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret)
        : "a"(SYSCALL_DUP2), "b"(oldfd), "c"(newfd) : "memory");
    return __syscall_fix(ret);
}

static int sys_pipe(int fds[2]) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret)
        : "a"(SYSCALL_PIPE), "b"(fds) : "memory");
    return __syscall_fix(ret);
}

static __attribute__((noreturn)) void sys_exit(int code) {
    __asm__ volatile("int $0x80" : : "a"(SYSCALL_EXIT), "b"(code) : "memory");
    for (;;) __asm__ volatile("hlt");
}

/* ---- string helpers ---- */

static uint32_t slen(const char* s) {
    uint32_t n = 0;
    while (s && s[n]) n++;
    return n;
}

static void wr(int fd, const char* s) {
    (void)sys_write(fd, s, slen(s));
}

static int scmp(const char* a, const char* b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

static void scpy(char* d, const char* s) {
    while (*s) *d++ = *s++;
    *d = 0;
}

/* ---- line reading ---- */

#define LINE_MAX 256
#define MAX_ARGS 32

static char line[LINE_MAX];

static int read_line(void) {
    uint32_t pos = 0;
    while (pos < LINE_MAX - 1) {
        char c;
        int r = sys_read(0, &c, 1);
        if (r <= 0) {
            if (pos == 0) return -1;
            break;
        }
        if (c == '\n' || c == '\r') break;
        if ((c == '\b' || c == 127) && pos > 0) { pos--; continue; }
        if (c >= ' ' && c <= '~') line[pos++] = c;
    }
    line[pos] = 0;
    return (int)pos;
}

/* ---- argument parsing ---- */

static int parse_args(char* cmd, char** argv, int max) {
    int argc = 0;
    char* p = cmd;
    while (*p && argc < max - 1) {
        while (*p == ' ' || *p == '\t') p++;
        if (*p == 0) break;
        argv[argc++] = p;
        while (*p && *p != ' ' && *p != '\t') p++;
        if (*p) *p++ = 0;
    }
    argv[argc] = 0;
    return argc;
}

/* ---- PATH resolution ---- */

static char pathbuf[256];

static const char* resolve(const char* cmd) {
    if (cmd[0] == '/' || cmd[0] == '.') return cmd;
    static const char* dirs[] = { "/bin/", "/disk/bin/", 0 };
    for (int i = 0; dirs[i]; i++) {
        scpy(pathbuf, dirs[i]);
        scpy(pathbuf + slen(pathbuf), cmd);
        int fd = sys_open(pathbuf, O_RDONLY);
        if (fd >= 0) { sys_close(fd); return pathbuf; }
    }
    return cmd;
}

/* ---- run a single simple command ---- */

static void run_simple(char* cmd) {
    char* argv[MAX_ARGS];
    int argc = parse_args(cmd, argv, MAX_ARGS);
    if (argc == 0) return;

    /* extract redirections */
    char* redir_out = 0;
    char* redir_in  = 0;
    int nargc = 0;
    for (int i = 0; i < argc; i++) {
        if (scmp(argv[i], ">") == 0 && i + 1 < argc) {
            redir_out = argv[++i];
        } else if (scmp(argv[i], "<") == 0 && i + 1 < argc) {
            redir_in = argv[++i];
        } else {
            argv[nargc++] = argv[i];
        }
    }
    argv[nargc] = 0;
    argc = nargc;
    if (argc == 0) return;

    /* builtin: exit */
    if (scmp(argv[0], "exit") == 0) {
        int code = 0;
        if (argc > 1) {
            const char* s = argv[1];
            while (*s >= '0' && *s <= '9') {
                code = code * 10 + (*s - '0');
                s++;
            }
        }
        sys_exit(code);
    }

    /* builtin: echo */
    if (scmp(argv[0], "echo") == 0) {
        for (int i = 1; i < argc; i++) {
            if (i > 1) wr(1, " ");
            wr(1, argv[i]);
        }
        wr(1, "\n");
        return;
    }

    /* external command */
    const char* path = resolve(argv[0]);
    int pid = sys_fork();
    if (pid < 0) { wr(2, "sh: fork failed\n"); return; }

    if (pid == 0) {
        /* child */
        if (redir_in) {
            int fd = sys_open(redir_in, O_RDONLY);
            if (fd >= 0) { sys_dup2(fd, 0); sys_close(fd); }
        }
        if (redir_out) {
            int fd = sys_open(redir_out, O_WRONLY | O_CREAT | O_TRUNC);
            if (fd >= 0) { sys_dup2(fd, 1); sys_close(fd); }
        }
        sys_execve(path, argv, 0);
        wr(2, "sh: ");
        wr(2, argv[0]);
        wr(2, ": not found\n");
        sys_exit(127);
    }

    /* parent waits */
    int st;
    sys_waitpid(pid, &st, 0);
}

/* ---- pipeline support ---- */

static void run_pipeline(char* cmdline) {
    /* split on '|' */
    char* cmds[4];
    int ncmds = 0;
    cmds[0] = cmdline;
    for (char* p = cmdline; *p; p++) {
        if (*p == '|' && ncmds < 3) {
            *p = 0;
            cmds[++ncmds] = p + 1;
        }
    }
    ncmds++;

    if (ncmds == 1) {
        run_simple(cmds[0]);
        return;
    }

    /* multi-stage pipeline */
    int prev_rd = -1;
    for (int i = 0; i < ncmds; i++) {
        int pfd[2] = {-1, -1};
        if (i < ncmds - 1) {
            if (sys_pipe(pfd) < 0) {
                wr(2, "sh: pipe failed\n");
                return;
            }
        }

        int pid = sys_fork();
        if (pid < 0) { wr(2, "sh: fork failed\n"); return; }

        if (pid == 0) {
            if (prev_rd >= 0) { sys_dup2(prev_rd, 0); sys_close(prev_rd); }
            if (pfd[1] >= 0)  { sys_dup2(pfd[1], 1); sys_close(pfd[1]); }
            if (pfd[0] >= 0)  sys_close(pfd[0]);

            char* argv[MAX_ARGS];
            int argc = parse_args(cmds[i], argv, MAX_ARGS);
            if (argc == 0) sys_exit(0);
            const char* path = resolve(argv[0]);
            sys_execve(path, argv, 0);
            wr(2, "sh: ");
            wr(2, argv[0]);
            wr(2, ": not found\n");
            sys_exit(127);
        }

        /* parent */
        if (prev_rd >= 0) sys_close(prev_rd);
        if (pfd[1] >= 0)  sys_close(pfd[1]);
        prev_rd = pfd[0];
    }

    if (prev_rd >= 0) sys_close(prev_rd);

    /* wait for all children */
    for (int i = 0; i < ncmds; i++) {
        int st;
        sys_waitpid(-1, &st, 0);
    }
}

/* ---- main loop ---- */

static void sh_main(void) {
    wr(1, "$ ");
    while (1) {
        int len = read_line();
        if (len < 0) break;
        if (len > 0) run_pipeline(line);
        wr(1, "$ ");
    }
}

__attribute__((naked)) void _start(void) {
    __asm__ volatile(
        "call sh_main\n"
        "mov $0, %ebx\n"
        "mov $2, %eax\n"
        "int $0x80\n"
        "hlt\n"
    );
}
