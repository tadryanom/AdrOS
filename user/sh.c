/* AdrOS POSIX-like shell (/bin/sh)
 *
 * Features:
 *   - Variable assignment (VAR=value) and expansion ($VAR)
 *   - Environment variables (export VAR=value)
 *   - Line editing (left/right arrow keys)
 *   - Command history (up/down arrow keys)
 *   - Pipes (cmd1 | cmd2 | cmd3)
 *   - Redirections (< > >>)
 *   - Operators: ; && || &
 *   - Job control: CTRL+C (SIGINT), CTRL+Z (SIGTSTP), background (&)
 *   - Builtins: cd, exit, echo, export, unset, set, pwd, type
 *   - PATH-based command resolution
 *   - Quote handling (single and double quotes)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <termios.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/wait.h>

static struct termios orig_termios;

static void tty_raw_mode(void) {
    tcgetattr(STDIN_FILENO, &orig_termios);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ICANON | ECHO | ISIG);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}

static void tty_restore(void) {
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
}

#define LINE_MAX   512
#define MAX_ARGS   64
#define MAX_VARS   64
#define HIST_SIZE  32

/* ---- Shell variables ---- */

static struct {
    char name[64];
    char value[256];
    int  exported;
} vars[MAX_VARS];
static int nvar = 0;

static int last_status = 0;

static const char* var_get(const char* name) {
    for (int i = 0; i < nvar; i++)
        if (strcmp(vars[i].name, name) == 0) return vars[i].value;
    return getenv(name);
}

static void var_set(const char* name, const char* value, int exported) {
    for (int i = 0; i < nvar; i++) {
        if (strcmp(vars[i].name, name) == 0) {
            strncpy(vars[i].value, value, 255);
            vars[i].value[255] = '\0';
            if (exported) vars[i].exported = 1;
            return;
        }
    }
    if (nvar < MAX_VARS) {
        strncpy(vars[nvar].name, name, 63);
        vars[nvar].name[63] = '\0';
        strncpy(vars[nvar].value, value, 255);
        vars[nvar].value[255] = '\0';
        vars[nvar].exported = exported;
        nvar++;
    }
}

static void var_unset(const char* name) {
    for (int i = 0; i < nvar; i++) {
        if (strcmp(vars[i].name, name) == 0) {
            vars[i] = vars[--nvar];
            return;
        }
    }
}

/* Build envp array from exported variables */
static char env_buf[MAX_VARS][320];
static char* envp_arr[MAX_VARS + 1];

static char** build_envp(void) {
    int n = 0;
    for (int i = 0; i < nvar && n < MAX_VARS; i++) {
        if (!vars[i].exported) continue;
        snprintf(env_buf[n], sizeof(env_buf[n]), "%s=%s",
                 vars[i].name, vars[i].value);
        envp_arr[n] = env_buf[n];
        n++;
    }
    envp_arr[n] = NULL;
    return envp_arr;
}

/* ---- Command history ---- */

static char history[HIST_SIZE][LINE_MAX];
static int hist_count = 0;
static int hist_pos = 0;

static void hist_add(const char* line) {
    if (line[0] == '\0') return;
    if (hist_count > 0 && strcmp(history[(hist_count - 1) % HIST_SIZE], line) == 0)
        return;
    strncpy(history[hist_count % HIST_SIZE], line, LINE_MAX - 1);
    history[hist_count % HIST_SIZE][LINE_MAX - 1] = '\0';
    hist_count++;
}

/* ---- Line editing ---- */

static char line[LINE_MAX];

static void term_write(const char* s, int n) {
    write(STDOUT_FILENO, s, (size_t)n);
}

/* ---- Tab completion ---- */

static int tab_complete(char* buf, int* p_pos, int* p_len) {
    int pos = *p_pos;
    int len = *p_len;

    /* Find the start of the current word */
    int wstart = pos;
    while (wstart > 0 && buf[wstart - 1] != ' ' && buf[wstart - 1] != '\t')
        wstart--;

    char prefix[128];
    int plen = pos - wstart;
    if (plen <= 0 || plen >= (int)sizeof(prefix)) return 0;
    memcpy(prefix, buf + wstart, (size_t)plen);
    prefix[plen] = '\0';

    /* Determine if this is a command (first word) or filename */
    int is_cmd = 1;
    for (int i = 0; i < wstart; i++) {
        if (buf[i] != ' ' && buf[i] != '\t') { is_cmd = 0; break; }
    }

    char match[128];
    match[0] = '\0';
    int nmatches = 0;

    /* Split prefix into directory part and name part for file completion */
    char dirpath[128] = ".";
    const char* namepfx = prefix;
    char* lastsep = NULL;
    for (char* p = prefix; *p; p++) {
        if (*p == '/') lastsep = p;
    }
    if (lastsep) {
        int dlen = (int)(lastsep - prefix);
        if (dlen == 0) { dirpath[0] = '/'; dirpath[1] = '\0'; }
        else { memcpy(dirpath, prefix, (size_t)dlen); dirpath[dlen] = '\0'; }
        namepfx = lastsep + 1;
    }
    int nplen = (int)strlen(namepfx);

    if (!is_cmd || lastsep) {
        /* File/directory completion */
        int fd = open(dirpath, 0);
        if (fd >= 0) {
            char dbuf[512];
            int rc;
            while ((rc = getdents(fd, dbuf, sizeof(dbuf))) > 0) {
                int off = 0;
                while (off < rc) {
                    struct dirent* d = (struct dirent*)(dbuf + off);
                    if (d->d_reclen == 0) break;
                    if (d->d_name[0] != '.' || nplen > 0) {
                        int nlen = (int)strlen(d->d_name);
                        if (nlen >= nplen && memcmp(d->d_name, namepfx, (size_t)nplen) == 0) {
                            if (nmatches == 0) strcpy(match, d->d_name);
                            nmatches++;
                        }
                    }
                    off += d->d_reclen;
                }
            }
            close(fd);
        }
    }

    if (is_cmd && !lastsep) {
        /* Command completion: search PATH directories + builtins */
        static const char* builtins[] = {
            "cd", "exit", "echo", "export", "unset", "set", "pwd", "type", NULL
        };
        for (int i = 0; builtins[i]; i++) {
            int blen = (int)strlen(builtins[i]);
            if (blen >= plen && memcmp(builtins[i], prefix, (size_t)plen) == 0) {
                if (nmatches == 0) strcpy(match, builtins[i]);
                nmatches++;
            }
        }
        const char* path_env = var_get("PATH");
        if (!path_env) path_env = "/bin:/sbin:/usr/bin";
        char pathcopy[512];
        strncpy(pathcopy, path_env, sizeof(pathcopy) - 1);
        pathcopy[sizeof(pathcopy) - 1] = '\0';
        char* save = pathcopy;
        char* dir;
        while ((dir = save) != NULL) {
            char* sep = strchr(save, ':');
            if (sep) { *sep = '\0'; save = sep + 1; } else save = NULL;
            int fd = open(dir, 0);
            if (fd < 0) continue;
            char dbuf[512];
            int rc;
            while ((rc = getdents(fd, dbuf, sizeof(dbuf))) > 0) {
                int off = 0;
                while (off < rc) {
                    struct dirent* d = (struct dirent*)(dbuf + off);
                    if (d->d_reclen == 0) break;
                    int nlen = (int)strlen(d->d_name);
                    if (nlen >= plen && memcmp(d->d_name, prefix, (size_t)plen) == 0) {
                        if (nmatches == 0) strcpy(match, d->d_name);
                        nmatches++;
                    }
                    off += d->d_reclen;
                }
            }
            close(fd);
        }
    }

    if (nmatches != 1) return 0;

    /* Insert the completion suffix */
    int mlen = (int)strlen(match);
    int suffix_len = is_cmd && !lastsep ? mlen - plen : mlen - nplen;
    const char* suffix = is_cmd && !lastsep ? match + plen : match + nplen;
    if (suffix_len <= 0 || len + suffix_len >= LINE_MAX - 1) return 0;

    memmove(buf + pos + suffix_len, buf + pos, (size_t)(len - pos));
    memcpy(buf + pos, suffix, (size_t)suffix_len);
    len += suffix_len;
    buf[len] = '\0';
    term_write(buf + pos, len - pos);
    pos += suffix_len;
    for (int i = 0; i < len - pos; i++) term_write("\b", 1);
    *p_pos = pos;
    *p_len = len;
    return 1;
}

static int read_line_edit(void) {
    int pos = 0;
    int len = 0;
    hist_pos = hist_count;

    memset(line, 0, LINE_MAX);

    while (len < LINE_MAX - 1) {
        char c;
        int r = read(STDIN_FILENO, &c, 1);
        if (r <= 0) {
            if (len == 0) return -1;
            break;
        }

        if (c == '\n' || c == '\r') {
            term_write("\n", 1);
            break;
        }

        /* Backspace / DEL */
        if (c == '\b' || c == 127) {
            if (pos > 0) {
                memmove(line + pos - 1, line + pos, (size_t)(len - pos));
                pos--; len--;
                line[len] = '\0';
                /* Redraw: move cursor back, print rest, clear tail */
                term_write("\b", 1);
                term_write(line + pos, len - pos);
                term_write(" \b", 2);
                for (int i = 0; i < len - pos; i++) term_write("\b", 1);
            }
            continue;
        }

        /* Tab = autocomplete */
        if (c == '\t') {
            tab_complete(line, &pos, &len);
            continue;
        }

        /* Ctrl+D = EOF */
        if (c == 4) {
            if (len == 0) return -1;
            continue;
        }

        /* Ctrl+C = cancel line */
        if (c == 3) {
            term_write("^C\n", 3);
            line[0] = '\0';
            return 0;
        }

        /* Ctrl+Z = ignored at prompt (no foreground job to suspend) */
        if (c == 26) {
            continue;
        }

        /* Ctrl+A = beginning of line */
        if (c == 1) {
            while (pos > 0) { term_write("\b", 1); pos--; }
            continue;
        }

        /* Ctrl+E = end of line */
        if (c == 5) {
            term_write(line + pos, len - pos);
            pos = len;
            continue;
        }

        /* Ctrl+U = clear line */
        if (c == 21) {
            while (pos > 0) { term_write("\b", 1); pos--; }
            for (int i = 0; i < len; i++) term_write(" ", 1);
            for (int i = 0; i < len; i++) term_write("\b", 1);
            len = 0; pos = 0;
            line[0] = '\0';
            continue;
        }

        /* Escape sequences (arrow keys) */
        if (c == 27) {
            char seq[2];
            if (read(STDIN_FILENO, &seq[0], 1) <= 0) continue;
            if (seq[0] != '[') continue;
            if (read(STDIN_FILENO, &seq[1], 1) <= 0) continue;

            if (seq[1] >= '0' && seq[1] <= '9') {
                /* Extended sequence like \x1b[3~ (DELETE), \x1b[1~ (Home), \x1b[4~ (End) */
                char trail;
                if (read(STDIN_FILENO, &trail, 1) <= 0) continue;
                if (trail == '~') {
                    if (seq[1] == '3') {
                        /* DELETE key — delete char at cursor */
                        if (pos < len) {
                            memmove(line + pos, line + pos + 1, (size_t)(len - pos - 1));
                            len--;
                            line[len] = '\0';
                            term_write(line + pos, len - pos);
                            term_write(" \b", 2);
                            for (int i = 0; i < len - pos; i++) term_write("\b", 1);
                        }
                    } else if (seq[1] == '1') {
                        /* Home */
                        while (pos > 0) { term_write("\b", 1); pos--; }
                    } else if (seq[1] == '4') {
                        /* End */
                        term_write(line + pos, len - pos);
                        pos = len;
                    }
                }
                continue;
            }

            switch (seq[1]) {
            case 'A':  /* Up arrow — previous history */
                if (hist_pos > 0 && hist_pos > hist_count - HIST_SIZE) {
                    hist_pos--;
                    /* Clear current line */
                    while (pos > 0) { term_write("\b", 1); pos--; }
                    for (int i = 0; i < len; i++) term_write(" ", 1);
                    for (int i = 0; i < len; i++) term_write("\b", 1);
                    /* Load history entry */
                    strcpy(line, history[hist_pos % HIST_SIZE]);
                    len = (int)strlen(line);
                    pos = len;
                    term_write(line, len);
                }
                break;
            case 'B':  /* Down arrow — next history */
                if (hist_pos < hist_count) {
                    hist_pos++;
                    while (pos > 0) { term_write("\b", 1); pos--; }
                    for (int i = 0; i < len; i++) term_write(" ", 1);
                    for (int i = 0; i < len; i++) term_write("\b", 1);
                    if (hist_pos < hist_count) {
                        strcpy(line, history[hist_pos % HIST_SIZE]);
                    } else {
                        line[0] = '\0';
                    }
                    len = (int)strlen(line);
                    pos = len;
                    term_write(line, len);
                }
                break;
            case 'C':  /* Right arrow */
                if (pos < len) { term_write(line + pos, 1); pos++; }
                break;
            case 'D':  /* Left arrow */
                if (pos > 0) { term_write("\b", 1); pos--; }
                break;
            case 'H':  /* Home */
                while (pos > 0) { term_write("\b", 1); pos--; }
                break;
            case 'F':  /* End */
                term_write(line + pos, len - pos);
                pos = len;
                break;
            }
            continue;
        }

        /* Normal printable character */
        if (c >= ' ' && c <= '~') {
            memmove(line + pos + 1, line + pos, (size_t)(len - pos));
            line[pos] = c;
            len++; line[len] = '\0';
            term_write(line + pos, len - pos);
            pos++;
            for (int i = 0; i < len - pos; i++) term_write("\b", 1);
        }
    }

    line[len] = '\0';
    return len;
}

/* ---- Variable expansion ---- */

static void expand_vars(const char* src, char* dst, int maxlen) {
    int di = 0;
    while (*src && di < maxlen - 1) {
        if (*src == '$') {
            src++;
            if (*src == '?') {
                di += snprintf(dst + di, (size_t)(maxlen - di), "%d", last_status);
                src++;
            } else if (*src == '{') {
                src++;
                char name[64];
                int ni = 0;
                while (*src && *src != '}' && ni < 63) name[ni++] = *src++;
                name[ni] = '\0';
                if (*src == '}') src++;
                const char* val = var_get(name);
                if (val) {
                    int vl = (int)strlen(val);
                    if (di + vl < maxlen) { memcpy(dst + di, val, (size_t)vl); di += vl; }
                }
            } else {
                char name[64];
                int ni = 0;
                while ((*src >= 'A' && *src <= 'Z') || (*src >= 'a' && *src <= 'z') ||
                       (*src >= '0' && *src <= '9') || *src == '_') {
                    if (ni < 63) name[ni++] = *src;
                    src++;
                }
                name[ni] = '\0';
                const char* val = var_get(name);
                if (val) {
                    int vl = (int)strlen(val);
                    if (di + vl < maxlen) { memcpy(dst + di, val, (size_t)vl); di += vl; }
                }
            }
        } else {
            dst[di++] = *src++;
        }
    }
    dst[di] = '\0';
}

/* ---- Argument parsing with quote handling ---- */

static int parse_args(char* cmd, char** argv, int max) {
    int argc = 0;
    char* p = cmd;
    while (*p && argc < max - 1) {
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0') break;

        char* out = p;
        argv[argc++] = out;

        while (*p && *p != ' ' && *p != '\t') {
            if (*p == '\'' ) {
                p++;
                while (*p && *p != '\'') *out++ = *p++;
                if (*p == '\'') p++;
            } else if (*p == '"') {
                p++;
                while (*p && *p != '"') *out++ = *p++;
                if (*p == '"') p++;
            } else {
                *out++ = *p++;
            }
        }
        if (*p) p++;
        *out = '\0';
    }
    argv[argc] = NULL;
    return argc;
}

/* ---- PATH resolution ---- */

static char pathbuf[256];

static const char* resolve(const char* cmd) {
    if (cmd[0] == '/' || cmd[0] == '.') return cmd;

    const char* path_env = var_get("PATH");
    if (!path_env) path_env = "/bin:/sbin:/usr/bin";

    char pathcopy[512];
    strncpy(pathcopy, path_env, sizeof(pathcopy) - 1);
    pathcopy[sizeof(pathcopy) - 1] = '\0';

    char* save = pathcopy;
    char* dir;
    while ((dir = save) != NULL) {
        char* sep = strchr(save, ':');
        if (sep) { *sep = '\0'; save = sep + 1; }
        else save = NULL;

        snprintf(pathbuf, sizeof(pathbuf), "%s/%s", dir, cmd);
        if (access(pathbuf, 0) == 0) return pathbuf;
    }
    return cmd;
}

/* ---- Helper: set foreground process group on controlling TTY ---- */

static void set_fg_pgrp(int pgrp) {
    ioctl(STDIN_FILENO, TIOCSPGRP, &pgrp);
}

/* ---- Run a single simple command ---- */

static int bg_flag = 0;  /* set by caller when trailing & detected */

static void run_simple(char* cmd) {
    /* Expand variables */
    char expanded[LINE_MAX];
    expand_vars(cmd, expanded, LINE_MAX);

    char* argv[MAX_ARGS];
    int argc = parse_args(expanded, argv, MAX_ARGS);
    if (argc == 0) return;

    /* Check for variable assignment (no command, just VAR=value) */
    if (argc == 1 && strchr(argv[0], '=') != NULL) {
        char* eq = strchr(argv[0], '=');
        *eq = '\0';
        var_set(argv[0], eq + 1, 0);
        last_status = 0;
        return;
    }

    /* Extract redirections */
    char* redir_out = NULL;
    char* redir_in  = NULL;
    int   append = 0;
    int   heredoc_fd = -1;
    int nargc = 0;
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], ">>") == 0 && i + 1 < argc) {
            redir_out = argv[++i]; append = 1;
        } else if (strcmp(argv[i], ">") == 0 && i + 1 < argc) {
            redir_out = argv[++i]; append = 0;
        } else if (strcmp(argv[i], "<<") == 0 && i + 1 < argc) {
            char* delim = argv[++i];
            int dlen = (int)strlen(delim);
            if (dlen > 0 && (delim[0] == '"' || delim[0] == '\'')) {
                delim++; dlen -= 2; if (dlen < 0) dlen = 0;
                delim[dlen] = '\0';
            }
            int pfd[2];
            if (pipe(pfd) == 0) {
                tty_restore();
                char hline[LINE_MAX];
                while (1) {
                    write(STDOUT_FILENO, "> ", 2);
                    int hi = 0;
                    char hc;
                    while (read(STDIN_FILENO, &hc, 1) == 1) {
                        if (hc == '\n') break;
                        if (hi < LINE_MAX - 1) hline[hi++] = hc;
                    }
                    hline[hi] = '\0';
                    if (strcmp(hline, delim) == 0) break;
                    write(pfd[1], hline, hi);
                    write(pfd[1], "\n", 1);
                }
                close(pfd[1]);
                heredoc_fd = pfd[0];
                tty_raw_mode();
            }
        } else if (strcmp(argv[i], "<") == 0 && i + 1 < argc) {
            redir_in = argv[++i];
        } else {
            argv[nargc++] = argv[i];
        }
    }
    argv[nargc] = NULL;
    argc = nargc;
    if (argc == 0) return;

    /* ---- Apply redirections for builtins too ---- */
    int saved_stdin = -1, saved_stdout = -1;
    if (heredoc_fd >= 0) {
        saved_stdin = dup(0); dup2(heredoc_fd, 0); close(heredoc_fd); heredoc_fd = -1;
    } else if (redir_in) {
        int fd = open(redir_in, O_RDONLY);
        if (fd >= 0) { saved_stdin = dup(0); dup2(fd, 0); close(fd); }
    }
    if (redir_out) {
        int flags = O_WRONLY | O_CREAT;
        flags |= append ? O_APPEND : O_TRUNC;
        int fd = open(redir_out, flags);
        if (fd >= 0) { saved_stdout = dup(1); dup2(fd, 1); close(fd); }
    }

    /* ---- Builtins ---- */

    if (strcmp(argv[0], "exit") == 0) {
        int code = argc > 1 ? atoi(argv[1]) : last_status;
        exit(code);
    }

    if (strcmp(argv[0], "cd") == 0) {
        const char* dir = argc > 1 ? argv[1] : var_get("HOME");
        if (!dir) dir = "/";
        if (chdir(dir) < 0)
            fprintf(stderr, "cd: %s: No such file or directory\n", dir);
        else {
            char cwd[256];
            if (getcwd(cwd, sizeof(cwd)) >= 0)
                var_set("PWD", cwd, 1);
        }
        goto restore_redir;
    }

    if (strcmp(argv[0], "pwd") == 0) {
        char cwd[256];
        if (getcwd(cwd, sizeof(cwd)) >= 0)
            printf("%s\n", cwd);
        else
            fprintf(stderr, "pwd: error\n");
        goto restore_redir;
    }

    if (strcmp(argv[0], "export") == 0) {
        for (int i = 1; i < argc; i++) {
            char* eq = strchr(argv[i], '=');
            if (eq) {
                *eq = '\0';
                var_set(argv[i], eq + 1, 1);
            } else {
                /* Export existing variable */
                for (int j = 0; j < nvar; j++)
                    if (strcmp(vars[j].name, argv[i]) == 0)
                        vars[j].exported = 1;
            }
        }
        goto restore_redir;
    }

    if (strcmp(argv[0], "unset") == 0) {
        for (int i = 1; i < argc; i++) var_unset(argv[i]);
        goto restore_redir;
    }

    if (strcmp(argv[0], "set") == 0) {
        for (int i = 0; i < nvar; i++)
            printf("%s=%s\n", vars[i].name, vars[i].value);
        goto restore_redir;
    }

    if (strcmp(argv[0], "echo") == 0) {
        int nflag = 0;
        int start = 1;
        if (argc > 1 && strcmp(argv[1], "-n") == 0) { nflag = 1; start = 2; }
        for (int i = start; i < argc; i++) {
            if (i > start) write(STDOUT_FILENO, " ", 1);
            write(STDOUT_FILENO, argv[i], strlen(argv[i]));
        }
        if (!nflag) write(STDOUT_FILENO, "\n", 1);
        goto restore_redir;
    }

    if (strcmp(argv[0], "type") == 0) {
        for (int i = 1; i < argc; i++) {
            if (strcmp(argv[i], "cd") == 0 || strcmp(argv[i], "exit") == 0 ||
                strcmp(argv[i], "echo") == 0 || strcmp(argv[i], "export") == 0 ||
                strcmp(argv[i], "unset") == 0 || strcmp(argv[i], "set") == 0 ||
                strcmp(argv[i], "pwd") == 0 || strcmp(argv[i], "type") == 0) {
                printf("%s is a shell builtin\n", argv[i]);
            } else {
                const char* path = resolve(argv[i]);
                if (strcmp(path, argv[i]) != 0)
                    printf("%s is %s\n", argv[i], path);
                else
                    printf("%s: not found\n", argv[i]);
            }
        }
        goto restore_redir;
    }

    /* ---- External command — restore parent redirections before fork ---- */
    const char* path = resolve(argv[0]);
    char** envp = build_envp();

    int pid = fork();
    if (pid < 0) { fprintf(stderr, "sh: fork failed\n"); return; }

    if (pid == 0) {
        /* child: own process group, restore default signals */
        setpgid(0, 0);
        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = (uintptr_t)SIG_DFL;
        sigaction(SIGINT, &sa, NULL);
        sigaction(SIGTSTP, &sa, NULL);
        sigaction(SIGQUIT, &sa, NULL);

        if (redir_in) {
            int fd = open(redir_in, O_RDONLY);
            if (fd >= 0) { dup2(fd, 0); close(fd); }
        }
        if (redir_out) {
            int flags = O_WRONLY | O_CREAT;
            flags |= append ? O_APPEND : O_TRUNC;
            int fd = open(redir_out, flags);
            if (fd >= 0) { dup2(fd, 1); close(fd); }
        }
        execve(path, (const char* const*)argv, (const char* const*)envp);
        fprintf(stderr, "sh: %s: not found\n", argv[0]);
        _exit(127);
    }

    /* parent */
    setpgid(pid, pid);

    if (bg_flag) {
        /* Background: don't wait, print job info */
        printf("[bg] %d\n", pid);
        last_status = 0;
    } else {
        /* Foreground: make child the fg process group, wait, then restore */
        set_fg_pgrp(pid);
        int st;
        waitpid(pid, &st, 0);
        set_fg_pgrp(getpgrp());
        last_status = st;
    }
    goto restore_redir;

restore_redir:
    if (saved_stdout >= 0) { dup2(saved_stdout, 1); close(saved_stdout); }
    if (saved_stdin >= 0)  { dup2(saved_stdin, 0);  close(saved_stdin);  }
}

/* ---- Pipeline support ---- */

static void run_pipeline(char* cmdline) {
    /* Split on '|' (outside quotes) */
    char* cmds[8];
    int ncmds = 0;
    cmds[0] = cmdline;
    int in_sq = 0, in_dq = 0;
    for (char* p = cmdline; *p; p++) {
        if (*p == '\'' && !in_dq) in_sq = !in_sq;
        else if (*p == '"' && !in_sq) in_dq = !in_dq;
        else if (*p == '|' && !in_sq && !in_dq && ncmds < 7) {
            if (*(p + 1) == '|') { p++; continue; }  /* skip || */
            *p = '\0';
            cmds[++ncmds] = p + 1;
        }
    }
    ncmds++;

    if (ncmds == 1) {
        run_simple(cmds[0]);
        return;
    }

    /* Multi-stage pipeline */
    int prev_rd = -1;
    int pids[8];
    int pgid = 0;  /* pipeline process group = first child's PID */

    for (int i = 0; i < ncmds; i++) {
        int pfd[2] = {-1, -1};
        if (i < ncmds - 1) {
            if (pipe(pfd) < 0) {
                fprintf(stderr, "sh: pipe failed\n");
                return;
            }
        }

        pids[i] = fork();
        if (pids[i] < 0) { fprintf(stderr, "sh: fork failed\n"); return; }

        if (pids[i] == 0) {
            /* child: join pipeline process group, restore signals */
            int mypgid = pgid ? pgid : getpid();
            setpgid(0, mypgid);
            struct sigaction sa;
            memset(&sa, 0, sizeof(sa));
            sa.sa_handler = (uintptr_t)SIG_DFL;
            sigaction(SIGINT, &sa, NULL);
            sigaction(SIGTSTP, &sa, NULL);
            sigaction(SIGQUIT, &sa, NULL);

            if (prev_rd >= 0) { dup2(prev_rd, 0); close(prev_rd); }
            if (pfd[1] >= 0)  { dup2(pfd[1], 1); close(pfd[1]); }
            if (pfd[0] >= 0)  close(pfd[0]);

            /* Expand and parse this pipeline stage */
            char expanded[LINE_MAX];
            expand_vars(cmds[i], expanded, LINE_MAX);
            char* argv[MAX_ARGS];
            int argc = parse_args(expanded, argv, MAX_ARGS);
            if (argc == 0) _exit(0);
            const char* path = resolve(argv[0]);
            char** envp = build_envp();
            execve(path, (const char* const*)argv, (const char* const*)envp);
            fprintf(stderr, "sh: %s: not found\n", argv[0]);
            _exit(127);
        }

        /* parent: set pipeline pgid */
        if (i == 0) pgid = pids[0];
        setpgid(pids[i], pgid);

        if (prev_rd >= 0) close(prev_rd);
        if (pfd[1] >= 0)  close(pfd[1]);
        prev_rd = pfd[0];
    }

    if (prev_rd >= 0) close(prev_rd);

    if (!bg_flag) {
        /* Foreground pipeline: make it fg, wait, restore */
        set_fg_pgrp(pgid);
        for (int i = 0; i < ncmds; i++) {
            int st;
            waitpid(pids[i], &st, 0);
            if (i == ncmds - 1) last_status = st;
        }
        set_fg_pgrp(getpgrp());
    } else {
        printf("[bg] %d\n", pgid);
        last_status = 0;
    }
}

/* ---- Process a command line (handle ;, &&, ||, &) ---- */

enum { OP_NONE = 0, OP_SEMI, OP_AND, OP_OR, OP_BG };

static void process_line(char* input) {
    char* p = input;

    while (*p) {
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0') break;

        /* Find the next operator outside quotes */
        char* start = p;
        int in_sq = 0, in_dq = 0;
        int op = OP_NONE;

        while (*p) {
            if (*p == '\'' && !in_dq) { in_sq = !in_sq; p++; continue; }
            if (*p == '"' && !in_sq)  { in_dq = !in_dq; p++; continue; }
            if (in_sq || in_dq) { p++; continue; }

            if (*p == '&' && *(p + 1) == '&') {
                *p = '\0'; p += 2; op = OP_AND; break;
            }
            if (*p == '|' && *(p + 1) == '|') {
                *p = '\0'; p += 2; op = OP_OR; break;
            }
            if (*p == ';') {
                *p = '\0'; p++; op = OP_SEMI; break;
            }
            if (*p == '&') {
                *p = '\0'; p++; op = OP_BG; break;
            }
            p++;
        }

        /* Trim leading whitespace from segment */
        while (*start == ' ' || *start == '\t') start++;
        if (*start != '\0') {
            if (op == OP_BG) {
                bg_flag = 1;
                run_pipeline(start);
                bg_flag = 0;
            } else {
                bg_flag = 0;
                run_pipeline(start);
            }
        }

        /* For &&: skip remaining commands if last failed */
        if (op == OP_AND && last_status != 0) {
            /* Skip until we hit || or ; or & or end */
            while (*p) {
                while (*p == ' ' || *p == '\t') p++;
                if (*p == '\0') break;
                int skip_sq = 0, skip_dq = 0;
                while (*p) {
                    if (*p == '\'' && !skip_dq) { skip_sq = !skip_sq; p++; continue; }
                    if (*p == '"' && !skip_sq)  { skip_dq = !skip_dq; p++; continue; }
                    if (skip_sq || skip_dq) { p++; continue; }
                    if (*p == '|' && *(p + 1) == '|') break;
                    if (*p == ';') break;
                    if (*p == '&' && *(p + 1) != '&') break;
                    if (*p == '&' && *(p + 1) == '&') { p += 2; continue; }
                    p++;
                }
                break;
            }
        }

        /* For ||: skip remaining commands if last succeeded */
        if (op == OP_OR && last_status == 0) {
            while (*p) {
                while (*p == ' ' || *p == '\t') p++;
                if (*p == '\0') break;
                int skip_sq = 0, skip_dq = 0;
                while (*p) {
                    if (*p == '\'' && !skip_dq) { skip_sq = !skip_sq; p++; continue; }
                    if (*p == '"' && !skip_sq)  { skip_dq = !skip_dq; p++; continue; }
                    if (skip_sq || skip_dq) { p++; continue; }
                    if (*p == '&' && *(p + 1) == '&') break;
                    if (*p == ';') break;
                    if (*p == '&' && *(p + 1) != '&') break;
                    if (*p == '|' && *(p + 1) == '|') { p += 2; continue; }
                    p++;
                }
                break;
            }
        }
    }
}

/* ---- Prompt ---- */

static void print_prompt(void) {
    const char* user = var_get("USER");
    const char* host = var_get("HOSTNAME");
    char cwd[256];

    if (!user) user = "root";
    if (!host) host = "adros";

    if (getcwd(cwd, sizeof(cwd)) < 0) strcpy(cwd, "?");

    printf("%s@%s:%s$ ", user, host, cwd);
    fflush(stdout);
}

/* ---- Main ---- */

int main(int argc, char** argv, char** envp) {
    (void)argc;
    (void)argv;

    /* Import environment variables */
    if (envp) {
        for (int i = 0; envp[i]; i++) {
            char* eq = strchr(envp[i], '=');
            if (eq) {
                char name[64];
                int nlen = (int)(eq - envp[i]);
                if (nlen > 63) nlen = 63;
                memcpy(name, envp[i], (size_t)nlen);
                name[nlen] = '\0';
                var_set(name, eq + 1, 1);
            }
        }
    }

    /* Set defaults */
    if (!var_get("PATH"))
        var_set("PATH", "/bin:/sbin:/usr/bin", 1);
    if (!var_get("HOME"))
        var_set("HOME", "/", 1);

    /* Job control: create session + process group, become fg */
    setsid();
    set_fg_pgrp(getpgrp());

    /* Ignore job control signals in the shell itself */
    struct sigaction sa_ign;
    memset(&sa_ign, 0, sizeof(sa_ign));
    sa_ign.sa_handler = (uintptr_t)SIG_IGN;
    sigaction(SIGINT, &sa_ign, NULL);
    sigaction(SIGTSTP, &sa_ign, NULL);
    sigaction(SIGQUIT, &sa_ign, NULL);

    tty_raw_mode();

    print_prompt();
    while (1) {
        int len = read_line_edit();
        if (len < 0) break;
        if (len > 0) {
            hist_add(line);
            tty_restore();
            process_line(line);
            tty_raw_mode();
        }
        print_prompt();
    }

    tty_restore();
    return last_status;
}
