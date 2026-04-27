// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

/* AdrOS awk utility — minimal: BEGIN/END, NF, NR, print with OFS, -v var=val */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <regex.h>

#define MAX_FIELDS 64
#define MAX_VARS   64
#define MAX_RULES  32

struct awk_var {
    char name[32];
    char value[256];
};

static struct awk_var vars[MAX_VARS];
static int nvars = 0;

static char field_sep = ' ';  /* -F separator */
static char OFS[32] = " ";    /* output field separator */
static int NR = 0;            /* record (line) number */
static int NF = 0;            /* number of fields in current record */
static char* fields[MAX_FIELDS]; /* field pointers into split buffer */

enum rule_type { RULE_BEGIN, RULE_END, RULE_PATTERN, RULE_NONE };

struct awk_rule {
    enum rule_type type;
    regex_t pattern_re;
    int has_re;
    char action[256];  /* simplified: "print" or "print $1,$2" etc */
};

static struct awk_rule rules[MAX_RULES];
static int nrules = 0;

static void var_set(const char* name, const char* value) {
    for (int i = 0; i < nvars; i++) {
        if (strcmp(vars[i].name, name) == 0) {
            strncpy(vars[i].value, value, sizeof(vars[i].value) - 1);
            return;
        }
    }
    if (nvars < MAX_VARS) {
        strncpy(vars[nvars].name, name, sizeof(vars[nvars].name) - 1);
        strncpy(vars[nvars].value, value, sizeof(vars[nvars].value) - 1);
        nvars++;
    }
}

static const char* var_get(const char* name) {
    if (strcmp(name, "NR") == 0) { static char buf[16]; snprintf(buf, sizeof(buf), "%d", NR); return buf; }
    if (strcmp(name, "NF") == 0) { static char buf[16]; snprintf(buf, sizeof(buf), "%d", NF); return buf; }
    if (strcmp(name, "OFS") == 0) return OFS;
    if (strcmp(name, "FS") == 0) { static char buf[2]; buf[0] = field_sep; buf[1] = '\0'; return buf; }
    for (int i = 0; i < nvars; i++) {
        if (strcmp(vars[i].name, name) == 0) return vars[i].value;
    }
    return "";
}

static void split_line(char* line) {
    NF = 0;
    char* p = line;
    while (*p && NF < MAX_FIELDS - 1) {
        while (*p && (*p == field_sep || *p == '\t')) p++;
        if (!*p) break;
        fields[NF++] = p;
        while (*p && *p != field_sep && *p != '\t') p++;
        if (*p) *p++ = '\0';
    }
}

static const char* g_orig_line;

static const char* resolve_field(const char* tok) {
    if (tok[0] == '$') {
        int n = atoi(tok + 1);
        if (n == 0) return g_orig_line ? g_orig_line : "";
        if (n > 0 && n <= NF) return fields[n - 1];
        return "";
    }
    /* Check for variable name */
    if ((tok[0] >= 'A' && tok[0] <= 'Z') || (tok[0] >= 'a' && tok[0] <= 'z') || tok[0] == '_') {
        return var_get(tok);
    }
    return tok;
}

static void do_print(const char* action, char* orig_line) {
    /* Parse print arguments: print $1, $2, NR, etc. */
    const char* p = action + 5; /* skip "print" */
    while (*p == ' ') p++;

    if (*p == '\0' || *p == '}') {
        /* print with no args = print $0 (whole line) */
        printf("%s\n", orig_line);
        return;
    }

    int first = 1;
    while (*p && *p != '}') {
        while (*p == ' ') p++;
        if (*p == ',' || *p == ' ') { p++; continue; }
        if (*p == '\0' || *p == '}') break;

        /* Extract token */
        char tok[64];
        int ti = 0;
        while (*p && *p != ',' && *p != ' ' && *p != '}' && ti < 63)
            tok[ti++] = *p++;
        tok[ti] = '\0';

        if (!first) printf("%s", OFS);
        first = 0;

        /* String literal */
        if (tok[0] == '"') {
            char* end = tok + strlen(tok) - 1;
            if (*end == '"') {
                tok[ti - 1] = '\0';
                printf("%s", tok + 1);
            } else {
                printf("%s", tok);
            }
        } else {
            printf("%s", resolve_field(tok));
        }
    }
    printf("\n");
}

static void execute_action(const char* action, char* orig_line) {
    if (strncmp(action, "print", 5) == 0) {
        do_print(action, orig_line);
    }
}

static void process_rules(char* line) {
    g_orig_line = line;
    split_line(line);
    for (int i = 0; i < nrules; i++) {
        if (rules[i].type == RULE_PATTERN) {
            if (rules[i].has_re) {
                if (regexec(&rules[i].pattern_re, line, 0, NULL, 0) != 0) continue;
            }
            execute_action(rules[i].action, line);
        } else if (rules[i].type == RULE_NONE) {
            execute_action(rules[i].action, line);
        }
    }
}

static void parse_program(const char* prog) {
    /* Very simplified awk program parser.
     * Supports: BEGIN{...} END{...} /pattern/{...} {...}
     * Actions: print, print $1,$2, print NR, etc.
     */
    const char* p = prog;
    while (*p) {
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == ';') p++;
        if (!*p) break;

        enum rule_type rtype = RULE_NONE;
        char pat[256] = "";

        if (strncmp(p, "BEGIN", 5) == 0 && (p[5] == '{' || p[5] == ' ')) {
            rtype = RULE_BEGIN;
            p += 5;
            while (*p == ' ') p++;
        } else if (strncmp(p, "END", 3) == 0 && (p[3] == '{' || p[3] == ' ')) {
            rtype = RULE_END;
            p += 3;
            while (*p == ' ') p++;
        } else if (*p == '/') {
            /* Pattern /regex/ */
            p++;
            int pi = 0;
            while (*p && *p != '/' && pi < 255) pat[pi++] = *p++;
            pat[pi] = '\0';
            if (*p == '/') p++;
            rtype = RULE_PATTERN;
            while (*p == ' ') p++;
        }

        if (*p == '{') {
            p++;
            /* Extract action body */
            char action[256];
            int ai = 0;
            int depth = 1;
            while (*p && depth > 0 && ai < 255) {
                if (*p == '{') depth++;
                else if (*p == '}') { depth--; if (depth == 0) break; }
                action[ai++] = *p++;
            }
            action[ai] = '\0';
            if (*p == '}') p++;

            if (nrules < MAX_RULES) {
                rules[nrules].type = rtype;
                rules[nrules].has_re = 0;
                strncpy(rules[nrules].action, action, sizeof(rules[nrules].action) - 1);
                if (rtype == RULE_PATTERN && pat[0]) {
                    if (regcomp(&rules[nrules].pattern_re, pat, REG_EXTENDED) == 0)
                        rules[nrules].has_re = 1;
                }
                nrules++;
            }
        } else {
            /* No braces: entire remaining text is action for RULE_NONE */
            if (nrules < MAX_RULES) {
                rules[nrules].type = rtype;
                rules[nrules].has_re = 0;
                strncpy(rules[nrules].action, p, sizeof(rules[nrules].action) - 1);
                if (rtype == RULE_PATTERN && pat[0]) {
                    if (regcomp(&rules[nrules].pattern_re, pat, REG_EXTENDED) == 0)
                        rules[nrules].has_re = 1;
                }
                nrules++;
            }
            break;
        }
    }
}

int main(int argc, char** argv) {
    int argi = 1;
    while (argi < argc && argv[argi][0] == '-') {
        if (strcmp(argv[argi], "-F") == 0 && argi + 1 < argc) {
            field_sep = argv[argi + 1][0];
            argi += 2;
        } else if (strcmp(argv[argi], "-v") == 0 && argi + 1 < argc) {
            char* eq = strchr(argv[argi + 1], '=');
            if (eq) {
                char name[32];
                int nlen = (int)(eq - argv[argi + 1]);
                if (nlen > 31) nlen = 31;
                memcpy(name, argv[argi + 1], (size_t)nlen);
                name[nlen] = '\0';
                var_set(name, eq + 1);
            }
            argi += 2;
        } else if (strcmp(argv[argi], "--") == 0) { argi++; break; }
        else { fprintf(stderr, "awk: invalid option '%s'\n", argv[argi]); return 1; }
    }

    if (argi >= argc) {
        fprintf(stderr, "Usage: awk [-F sep] [-v var=val] 'program' [file]\n");
        return 1;
    }

    const char* prog = argv[argi++];
    parse_program(prog);

    /* Execute BEGIN rules */
    for (int i = 0; i < nrules; i++) {
        if (rules[i].type == RULE_BEGIN)
            execute_action(rules[i].action, "");
    }

    int fd = STDIN_FILENO;
    if (argi < argc) {
        fd = open(argv[argi], O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "awk: %s: No such file or directory\n", argv[argi]);
            return 1;
        }
    }

    char line[4096];
    char orig[4096];
    int li = 0;
    char c;
    while (read(fd, &c, 1) == 1) {
        if (c == '\n') {
            line[li] = '\0';
            strcpy(orig, line);
            NR++;
            process_rules(line);
            li = 0;
        } else if (li < (int)sizeof(line) - 1) {
            line[li++] = c;
        }
    }
    if (li > 0) {
        line[li] = '\0';
        strcpy(orig, line);
        NR++;
        process_rules(line);
    }

    /* Execute END rules */
    for (int i = 0; i < nrules; i++) {
        if (rules[i].type == RULE_END)
            execute_action(rules[i].action, "");
    }

    for (int i = 0; i < nrules; i++) {
        if (rules[i].has_re) regfree(&rules[i].pattern_re);
    }
    if (fd != STDIN_FILENO) close(fd);
    return 0;
}
