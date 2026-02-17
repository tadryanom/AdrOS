/* AdrOS sed utility — minimal stream editor (s/pattern/replacement/g only) */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

static int match_at(const char* s, const char* pat, int patlen) {
    for (int i = 0; i < patlen; i++) {
        if (s[i] == '\0' || s[i] != pat[i]) return 0;
    }
    return 1;
}

static void sed_substitute(const char* line, const char* pat, int patlen,
                           const char* rep, int replen, int global) {
    const char* p = line;
    while (*p) {
        if (match_at(p, pat, patlen)) {
            write(STDOUT_FILENO, rep, replen);
            p += patlen;
            if (!global) {
                write(STDOUT_FILENO, p, strlen(p));
                return;
            }
        } else {
            write(STDOUT_FILENO, p, 1);
            p++;
        }
    }
}

static int parse_s_cmd(const char* expr, char* pat, int* patlen,
                       char* rep, int* replen, int* global) {
    if (expr[0] != 's' || expr[1] == '\0') return -1;
    char delim = expr[1];
    const char* p = expr + 2;
    int pi = 0;
    while (*p && *p != delim && pi < 255) pat[pi++] = *p++;
    pat[pi] = '\0'; *patlen = pi;
    if (*p != delim) return -1;
    p++;
    int ri = 0;
    while (*p && *p != delim && ri < 255) rep[ri++] = *p++;
    rep[ri] = '\0'; *replen = ri;
    *global = 0;
    if (*p == delim) { p++; if (*p == 'g') *global = 1; }
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: sed 's/pattern/replacement/[g]' [file]\n");
        return 1;
    }

    char pat[256], rep[256];
    int patlen, replen, global;
    int ei = 1;
    if (strcmp(argv[1], "-e") == 0 && argc > 2) ei = 2;

    if (parse_s_cmd(argv[ei], pat, &patlen, rep, &replen, &global) < 0) {
        fprintf(stderr, "sed: invalid expression: %s\n", argv[ei]);
        return 1;
    }

    int fd = STDIN_FILENO;
    if (argc > ei + 1) {
        fd = open(argv[ei + 1], O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "sed: %s: No such file or directory\n", argv[ei + 1]);
            return 1;
        }
    }

    char line[4096];
    int li = 0;
    char c;
    while (read(fd, &c, 1) == 1) {
        if (c == '\n') {
            line[li] = '\0';
            sed_substitute(line, pat, patlen, rep, replen, global);
            write(STDOUT_FILENO, "\n", 1);
            li = 0;
        } else if (li < (int)sizeof(line) - 1) {
            line[li++] = c;
        }
    }
    if (li > 0) {
        line[li] = '\0';
        sed_substitute(line, pat, patlen, rep, replen, global);
    }

    if (fd != STDIN_FILENO) close(fd);
    return 0;
}
