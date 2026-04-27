// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

/* AdrOS sed utility — stream editor with address support and multiple commands */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <regex.h>

#define MAX_CMDS 64
#define MAX_TEXT 256

enum cmd_type { CMD_S, CMD_D, CMD_P, CMD_Q, CMD_A, CMD_I, CMD_C, CMD_Y };

struct addr {
    int type;       /* 0=none, 1=line, 2=last line ($), 3=regex */
    int line;
    regex_t re;
    int has_re;
};

struct sed_cmd {
    struct addr addr1;
    struct addr addr2;  /* second address (range) */
    int has_addr2;
    enum cmd_type cmd;
    /* s command */
    char s_pat[256];
    char s_rep[256];
    int  s_global;
    regex_t s_re;
    int  s_has_re;
    /* a/i/c text */
    char text[MAX_TEXT];
    /* y command */
    char y_src[256];
    char y_dst[256];
};

static struct sed_cmd cmds[MAX_CMDS];
static int ncmds = 0;
static int nflag = 0;  /* -n: suppress auto-print */

static int parse_addr(const char** pp, struct addr* a) {
    a->type = 0;
    a->has_re = 0;
    const char* p = *pp;
    if (*p == '$') { a->type = 2; p++; *pp = p; return 0; }
    if (*p == '/') {
        p++;
        char pat[256]; int pi = 0;
        while (*p && *p != '/' && pi < 255) pat[pi++] = *p++;
        pat[pi] = '\0';
        if (*p == '/') p++;
        if (regcomp(&a->re, pat, 0) != 0) return -1;
        a->has_re = 1;
        a->type = 3;
        *pp = p;
        return 0;
    }
    if (*p >= '0' && *p <= '9') {
        int n = 0;
        while (*p >= '0' && *p <= '9') n = n * 10 + (*p++ - '0');
        a->type = 1;
        a->line = n;
        *pp = p;
        return 0;
    }
    *pp = p;
    return 0;
}

static int parse_cmd(const char* expr, struct sed_cmd* c) {
    memset(c, 0, sizeof(*c));
    const char* p = expr;
    while (*p == ' ' || *p == '\t') p++;
    /* Parse first address */
    if (*p && *p != 's' && *p != 'd' && *p != 'p' && *p != 'q' &&
        *p != 'a' && *p != 'i' && *p != 'c' && *p != 'y') {
        if (parse_addr(&p, &c->addr1) < 0) return -1;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == ',') {
            p++;
            while (*p == ' ' || *p == '\t') p++;
            if (parse_addr(&p, &c->addr2) < 0) return -1;
            c->has_addr2 = 1;
            while (*p == ' ' || *p == '\t') p++;
        }
    }
    if (!*p) return -1;
    switch (*p) {
    case 's': {
        c->cmd = CMD_S;
        p++;
        if (!*p) return -1;
        char delim = *p++;
        int pi = 0;
        while (*p && *p != delim && pi < 255) c->s_pat[pi++] = *p++;
        c->s_pat[pi] = '\0';
        if (*p != delim) return -1;
        p++;
        int ri = 0;
        while (*p && *p != delim && ri < 255) c->s_rep[ri++] = *p++;
        c->s_rep[ri] = '\0';
        if (*p == delim) { p++; if (*p == 'g') c->s_global = 1; }
        if (regcomp(&c->s_re, c->s_pat, 0) != 0) return -1;
        c->s_has_re = 1;
        break;
    }
    case 'd': c->cmd = CMD_D; p++; break;
    case 'p': c->cmd = CMD_P; p++; break;
    case 'q': c->cmd = CMD_Q; p++; break;
    case 'a':
        c->cmd = CMD_A; p++;
        while (*p == ' ' || *p == '\t' || *p == '\\') p++;
        strncpy(c->text, p, MAX_TEXT - 1);
        c->text[MAX_TEXT - 1] = '\0';
        break;
    case 'i':
        c->cmd = CMD_I; p++;
        while (*p == ' ' || *p == '\t' || *p == '\\') p++;
        strncpy(c->text, p, MAX_TEXT - 1);
        c->text[MAX_TEXT - 1] = '\0';
        break;
    case 'c':
        c->cmd = CMD_C; p++;
        while (*p == ' ' || *p == '\t' || *p == '\\') p++;
        strncpy(c->text, p, MAX_TEXT - 1);
        c->text[MAX_TEXT - 1] = '\0';
        break;
    case 'y': {
        c->cmd = CMD_Y; p++;
        if (!*p) return -1;
        char delim = *p++;
        int si = 0;
        while (*p && *p != delim && si < 255) c->y_src[si++] = *p++;
        c->y_src[si] = '\0';
        if (*p != delim) return -1;
        p++;
        int di = 0;
        while (*p && *p != delim && di < 255) c->y_dst[di++] = *p++;
        c->y_dst[di] = '\0';
        break;
    }
    default: return -1;
    }
    return 0;
}

static int addr_match(struct addr* a, int lnum, int lastline, const char* line) {
    if (a->type == 0) return 1;
    if (a->type == 1) return (lnum == a->line);
    if (a->type == 2) return lastline;
    if (a->type == 3 && a->has_re)
        return (regexec(&a->re, line, 0, NULL, 0) == 0);
    return 0;
}

static int cmd_match(struct sed_cmd* c, int lnum, int lastline, const char* line) {
    if (c->addr1.type == 0) return 1;
    int m1 = addr_match(&c->addr1, lnum, lastline, line);
    if (!c->has_addr2) return m1;
    /* Range: match from addr1 to addr2 */
    static int in_range[MAX_CMDS];
    if (m1 && !in_range[c - cmds]) { in_range[c - cmds] = 1; return 1; }
    if (in_range[c - cmds]) {
        int m2 = addr_match(&c->addr2, lnum, lastline, line);
        if (m2) { in_range[c - cmds] = 0; return 1; }
        return 1;
    }
    return 0;
}

static void do_s(struct sed_cmd* c, char* line) {
    regmatch_t match;
    if (regexec(&c->s_re, line, 1, &match, 0) != 0) return;
    char out[4096];
    int oi = 0;
    int again = 1;
    int offset = 0;
    while (again) {
        again = 0;
        if (regexec(&c->s_re, line + offset, 1, &match, 0) != 0) break;
        for (int i = offset; i < offset + match.rm_so && oi < 4095; i++)
            out[oi++] = line[i];
        for (int i = 0; c->s_rep[i] && oi < 4095; i++)
            out[oi++] = c->s_rep[i];
        offset += match.rm_eo;
        if (c->s_global) again = 1;
    }
    for (int i = offset; line[i] && oi < 4095; i++)
        out[oi++] = line[i];
    out[oi] = '\0';
    strcpy(line, out);
}

static void do_y(struct sed_cmd* c, char* line) {
    int slen = (int)strlen(c->y_src);
    for (int i = 0; line[i]; i++) {
        for (int j = 0; j < slen; j++) {
            if (line[i] == c->y_src[j]) {
                line[i] = c->y_dst[j];
                break;
            }
        }
    }
}

int main(int argc, char** argv) {
    int ei = 1;
    while (ei < argc && argv[ei][0] == '-') {
        if (strcmp(argv[ei], "-n") == 0) nflag = 1;
        else if (strcmp(argv[ei], "-e") == 0) { ei++; break; }
        else if (strcmp(argv[ei], "--") == 0) { ei++; break; }
        else { fprintf(stderr, "sed: invalid option '%s'\n", argv[ei]); return 1; }
        ei++;
    }
    if (ei >= argc) {
        fprintf(stderr, "Usage: sed [-n] [-e] 'command' [file]\n");
        return 1;
    }

    /* Parse commands (semicolon-separated) */
    const char* expr = argv[ei++];
    char ebuf[1024];
    strncpy(ebuf, expr, sizeof(ebuf) - 1);
    ebuf[sizeof(ebuf) - 1] = '\0';
    char* tok = ebuf;
    while (tok && *tok) {
        char* semi = strchr(tok, ';');
        if (semi) *semi++ = '\0';
        while (*tok == ' ' || *tok == '\t') tok++;
        if (*tok && ncmds < MAX_CMDS) {
            if (parse_cmd(tok, &cmds[ncmds]) < 0) {
                fprintf(stderr, "sed: invalid command: %s\n", tok);
                return 1;
            }
            ncmds++;
        }
        tok = semi;
    }

    int fd = STDIN_FILENO;
    if (ei < argc) {
        fd = open(argv[ei], O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "sed: %s: No such file or directory\n", argv[ei]);
            return 1;
        }
    }

    char line[4096];
    int li = 0, lnum = 0;
    char c;
    while (read(fd, &c, 1) == 1) {
        if (c == '\n') {
            line[li] = '\0';
            lnum++;
            int deleted = 0;
            int quit = 0;
            for (int i = 0; i < ncmds; i++) {
                if (!cmd_match(&cmds[i], lnum, 0, line)) continue;
                switch (cmds[i].cmd) {
                case CMD_S: do_s(&cmds[i], line); break;
                case CMD_D: deleted = 1; break;
                case CMD_P: printf("%s\n", line); break;
                case CMD_Q: quit = 1; break;
                case CMD_A: printf("%s\n", cmds[i].text); break;
                case CMD_I: printf("%s\n", cmds[i].text); break;
                case CMD_C: printf("%s\n", cmds[i].text); deleted = 1; break;
                case CMD_Y: do_y(&cmds[i], line); break;
                }
                if (deleted || quit) break;
            }
            if (!deleted && !nflag) printf("%s\n", line);
            if (quit) goto done;
            li = 0;
        } else if (li < (int)sizeof(line) - 1) {
            line[li++] = c;
        }
    }
    if (li > 0) {
        line[li] = '\0';
        lnum++;
        int deleted = 0;
        for (int i = 0; i < ncmds; i++) {
            if (!cmd_match(&cmds[i], lnum, 1, line)) continue;
            switch (cmds[i].cmd) {
            case CMD_S: do_s(&cmds[i], line); break;
            case CMD_D: deleted = 1; break;
            case CMD_P: printf("%s\n", line); break;
            case CMD_A: printf("%s\n", cmds[i].text); break;
            case CMD_I: printf("%s\n", cmds[i].text); break;
            case CMD_C: printf("%s\n", cmds[i].text); deleted = 1; break;
            case CMD_Y: do_y(&cmds[i], line); break;
            default: break;
            }
            if (deleted) break;
        }
        if (!deleted && !nflag) printf("%s\n", line);
    }
done:
    for (int i = 0; i < ncmds; i++) {
        if (cmds[i].addr1.has_re) regfree(&cmds[i].addr1.re);
        if (cmds[i].has_addr2 && cmds[i].addr2.has_re) regfree(&cmds[i].addr2.re);
        if (cmds[i].s_has_re) regfree(&cmds[i].s_re);
    }
    if (fd != STDIN_FILENO) close(fd);
    return 0;
}
