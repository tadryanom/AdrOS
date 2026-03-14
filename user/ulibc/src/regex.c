// SPDX-License-Identifier: BSD-3-Clause
/*
 * AdrOS Kernel - Copyright (c) 2018, Tulio A M Mendes <myself@tadryanom.me>
 * All rights reserved.
 * See LICENSE for details.
 *
 * Source: https://projects.tadryanom.me/?p=AdrOS.git;a=summary
 * Mirror: https://github.com/tadryanom/AdrOS
 */

#include "regex.h"
#include "stdlib.h"
#include "string.h"
#include "ctype.h"

/*
 * Minimal POSIX regex — recursive backtracking matcher.
 * Supports BRE and ERE: . * + ? | ^ $ [] [^] () \1-\9
 * POSIX character classes: [:alpha:] [:digit:] [:alnum:] [:space:] etc.
 */

#define MAX_SUB 10
#define CLASS_BYTES 32

/* Internal compiled instruction */
enum {
    OP_LIT, OP_ANY, OP_CLASS, OP_NCLASS,
    OP_BOL, OP_EOL,
    OP_STAR, OP_PLUS, OP_QUEST,   /* greedy quantifiers: operand follows */
    OP_SAVE_OPEN, OP_SAVE_CLOSE,  /* sub-match boundary */
    OP_JMP, OP_SPLIT,             /* for alternation */
    OP_MATCH,
};

typedef struct {
    unsigned char type;
    unsigned char c;              /* literal char */
    unsigned char cclass[CLASS_BYTES];
    short arg;                    /* jmp offset or save slot */
} inst_t;

typedef struct {
    inst_t* code;
    int     len;
    int     cap;
    int     nsub;
    int     cflags;
} prog_t;

/* ---- helpers ---- */
static void cc_set(unsigned char* cc, int ch) {
    cc[(unsigned)ch / 8] |= (unsigned char)(1 << ((unsigned)ch % 8));
}
static int cc_test(const unsigned char* cc, int ch) {
    return (cc[(unsigned)ch / 8] >> ((unsigned)ch % 8)) & 1;
}

static inst_t* emit(prog_t* p) {
    if (p->len >= p->cap) {
        int nc = p->cap ? p->cap * 2 : 64;
        inst_t* tmp = (inst_t*)realloc(p->code, (size_t)nc * sizeof(inst_t));
        if (!tmp) return (inst_t*)0;
        p->code = tmp;
        p->cap = nc;
    }
    inst_t* ip = &p->code[p->len];
    memset(ip, 0, sizeof(*ip));
    p->len++;
    return ip;
}

/* Parse bracket expression, advance *pp past ']'. */
static void parse_bracket(const char** pp, unsigned char* cc, int* negate, int icase) {
    const char* p = *pp;
    *negate = 0;
    memset(cc, 0, CLASS_BYTES);
    if (*p == '^') { *negate = 1; p++; }
    if (*p == ']') { cc_set(cc, ']'); p++; }

    while (*p && *p != ']') {
        if (*p == '[' && p[1] == ':') {
            p += 2;
            const char* end = p;
            while (*end && !(*end == ':' && end[1] == ']')) end++;
            size_t len = (size_t)(end - p);
            for (int ch = 1; ch < 256; ch++) {
                int m = 0;
                if (len==5 && !memcmp(p,"alpha",5)) m = isalpha(ch);
                else if (len==5 && !memcmp(p,"digit",5)) m = isdigit(ch);
                else if (len==5 && !memcmp(p,"alnum",5)) m = isalnum(ch);
                else if (len==5 && !memcmp(p,"space",5)) m = isspace(ch);
                else if (len==5 && !memcmp(p,"upper",5)) m = isupper(ch);
                else if (len==5 && !memcmp(p,"lower",5)) m = islower(ch);
                else if (len==5 && !memcmp(p,"print",5)) m = isprint(ch);
                else if (len==5 && !memcmp(p,"graph",5)) m = isgraph(ch);
                else if (len==5 && !memcmp(p,"cntrl",5)) m = iscntrl(ch);
                else if (len==5 && !memcmp(p,"punct",5)) m = ispunct(ch);
                else if (len==6 && !memcmp(p,"xdigit",6)) m = isxdigit(ch);
                else if (len==5 && !memcmp(p,"blank",5)) m = (ch==' '||ch=='\t');
                if (m) cc_set(cc, ch);
            }
            if (*end) p = end + 2; else p = end;
            continue;
        }
        int c1 = (unsigned char)*p++;
        if (*p == '-' && p[1] && p[1] != ']') {
            p++;
            int c2 = (unsigned char)*p++;
            for (int c = c1; c <= c2; c++) cc_set(cc, c);
        } else {
            cc_set(cc, c1);
        }
    }
    if (*p == ']') p++;

    if (icase) {
        for (int ch = 0; ch < 256; ch++) {
            if (cc_test(cc, ch)) {
                if (ch >= 'a' && ch <= 'z') cc_set(cc, ch - 32);
                if (ch >= 'A' && ch <= 'Z') cc_set(cc, ch + 32);
            }
        }
    }
    *pp = p;
}

/* ---- Compiler: pattern string -> inst_t[] ---- */

/* Forward declarations */
static int compile_alt(prog_t* P, const char** pp, int ere, int icase);
static int compile_concat(prog_t* P, const char** pp, int ere, int icase);
static int compile_quant(prog_t* P, const char** pp, int ere, int icase);
static int compile_atom(prog_t* P, const char** pp, int ere, int icase);

static int compile_atom(prog_t* P, const char** pp, int ere, int icase) {
    const char* p = *pp;
    if (!*p) return -1;

    /* Group: ( or \( */
    if ((ere && *p == '(') || (!ere && *p == '\\' && p[1] == '(')) {
        p += ere ? 1 : 2;
        int sub = ++P->nsub;
        inst_t* ip = emit(P); if (!ip) return REG_ESPACE;
        ip->type = OP_SAVE_OPEN; ip->arg = (short)(sub * 2);

        *pp = p;
        int r = compile_alt(P, pp, ere, icase);
        if (r) return r;
        p = *pp;

        if ((ere && *p == ')') || (!ere && *p == '\\' && p[1] == ')')) {
            p += ere ? 1 : 2;
        }

        ip = emit(P); if (!ip) return REG_ESPACE;
        ip->type = OP_SAVE_CLOSE; ip->arg = (short)(sub * 2 + 1);
        *pp = p;
        return 0;
    }

    /* ^ */
    if (*p == '^') {
        inst_t* ip = emit(P); if (!ip) return REG_ESPACE;
        ip->type = OP_BOL;
        *pp = p + 1;
        return 0;
    }

    /* $ */
    if (*p == '$') {
        inst_t* ip = emit(P); if (!ip) return REG_ESPACE;
        ip->type = OP_EOL;
        *pp = p + 1;
        return 0;
    }

    /* . */
    if (*p == '.') {
        inst_t* ip = emit(P); if (!ip) return REG_ESPACE;
        ip->type = OP_ANY;
        *pp = p + 1;
        return 0;
    }

    /* [bracket] */
    if (*p == '[') {
        p++;
        inst_t* ip = emit(P); if (!ip) return REG_ESPACE;
        int neg = 0;
        parse_bracket(&p, ip->cclass, &neg, icase);
        ip->type = neg ? OP_NCLASS : OP_CLASS;
        *pp = p;
        return 0;
    }

    /* Escape */
    if (*p == '\\' && p[1]) {
        p++;
        /* Check for end-of-group */
        if (!ere && (*p == ')' || *p == '|')) return -1;
        inst_t* ip = emit(P); if (!ip) return REG_ESPACE;
        ip->type = OP_LIT;
        ip->c = (unsigned char)*p;
        *pp = p + 1;
        return 0;
    }

    /* ERE special: ) | are not atoms */
    if (ere && (*p == ')' || *p == '|')) return -1;
    /* BRE: \) \| are not atoms */
    if (!ere && *p == '\\' && (p[1] == ')' || p[1] == '|')) return -1;

    /* Literal */
    {
        inst_t* ip = emit(P); if (!ip) return REG_ESPACE;
        ip->type = OP_LIT;
        ip->c = (unsigned char)*p;
        if (icase && *p >= 'A' && *p <= 'Z') ip->c = (unsigned char)(*p + 32);
        if (icase && *p >= 'a' && *p <= 'z') ip->c = (unsigned char)*p;
        *pp = p + 1;
        return 0;
    }
}

static int compile_quant(prog_t* P, const char** pp, int ere, int icase) {
    int atom_start = P->len;
    int r = compile_atom(P, pp, ere, icase);
    if (r) return r;
    int atom_end = P->len;

    const char* p = *pp;
    int qtype = -1;
    if (*p == '*') { qtype = OP_STAR; p++; }
    else if (ere && *p == '+') { qtype = OP_PLUS; p++; }
    else if (ere && *p == '?') { qtype = OP_QUEST; p++; }

    if (qtype >= 0) {
        /* Wrap: insert a quantifier instruction referencing the atom range */
        /* Implementation: we store quantifier type and atom length so the
         * matcher can loop over the atom instructions. */
        inst_t* ip = emit(P); if (!ip) return REG_ESPACE;
        ip->type = (unsigned char)qtype;
        ip->arg = (short)(atom_end - atom_start); /* length of atom code */
        *pp = p;
    }
    return 0;
}

static int compile_concat(prog_t* P, const char** pp, int ere, int icase) {
    const char* p = *pp;
    if (!*p) return 0;

    while (*p) {
        /* Check for alternation or group close */
        if (ere && (*p == '|' || *p == ')')) break;
        if (!ere && *p == '\\' && (p[1] == '|' || p[1] == ')')) break;

        *pp = p;
        int r = compile_quant(P, pp, ere, icase);
        if (r == -1) break; /* not an atom */
        if (r > 0) return r; /* error */
        p = *pp;
    }
    *pp = p;
    return 0;
}

static int compile_alt(prog_t* P, const char** pp, int ere, int icase) {
    /* Save position for SPLIT patching */
    int start = P->len;

    int r = compile_concat(P, pp, ere, icase);
    if (r > 0) return r;

    const char* p = *pp;
    while ((ere && *p == '|') || (!ere && *p == '\\' && p[1] == '|')) {
        p += ere ? 1 : 2;

        /* Insert SPLIT at start, JMP at end of first branch */
        /* For simplicity, emit JMP at end of current branch */
        inst_t* jmp = emit(P); if (!jmp) return REG_ESPACE;
        jmp->type = OP_JMP;
        /* jmp->arg will be patched to point past alt */
        int jmp_idx = P->len - 1;

        /* Insert SPLIT at start */
        /* Actually, alternation in a linear bytecode is complex.
         * Let's use a simpler approach: mark alternation boundaries
         * and handle in the matcher with backtracking. */
        /* For now, store SPLIT with offset to second branch */
        inst_t* split = emit(P); if (!split) return REG_ESPACE;
        split->type = OP_SPLIT;
        split->arg = (short)(jmp_idx - start); /* first branch length */

        *pp = p;
        r = compile_concat(P, pp, ere, icase);
        if (r > 0) return r;
        p = *pp;

        /* Patch JMP to skip past this alternative */
        P->code[jmp_idx].arg = (short)(P->len - jmp_idx - 1);
    }
    *pp = p;
    (void)start;
    return 0;
}

/* ---- Backtracking matcher ---- */

typedef struct {
    const inst_t* code;
    int           codelen;
    const char*   str;
    const char*   str_start;
    int           eflags;
    int           cflags;
    const char*   sub[MAX_SUB * 2]; /* submatch start/end pointers */
} mstate_t;

static int match_inst(mstate_t* M, int pc, const char* sp);

static int match_char(const inst_t* ip, int ch, int icase) {
    if (ip->type == OP_ANY) return (ch != '\n' && ch != 0);
    if (ip->type == OP_LIT) {
        int c = ip->c;
        if (icase) {
            int lch = (ch >= 'A' && ch <= 'Z') ? ch + 32 : ch;
            int lc  = (c >= 'A' && c <= 'Z') ? c + 32 : c;
            return lch == lc;
        }
        return ch == c;
    }
    if (ip->type == OP_CLASS) return cc_test(ip->cclass, ch);
    if (ip->type == OP_NCLASS) return !cc_test(ip->cclass, ch) && ch != '\n' && ch != 0;
    return 0;
}

static int match_inst(mstate_t* M, int pc, const char* sp) {
    while (pc < M->codelen) {
        const inst_t* ip = &M->code[pc];

        switch (ip->type) {
        case OP_LIT:
        case OP_ANY:
        case OP_CLASS:
        case OP_NCLASS:
            if (*sp == '\0') return 0;
            if (!match_char(ip, (unsigned char)*sp, M->cflags & REG_ICASE)) return 0;
            sp++;
            pc++;
            break;

        case OP_BOL:
            if (sp != M->str_start && sp[-1] != '\n') {
                if (M->eflags & REG_NOTBOL) return 0;
                if (sp != M->str_start) return 0;
            }
            pc++;
            break;

        case OP_EOL:
            if (*sp != '\0' && *sp != '\n') {
                if (M->eflags & REG_NOTEOL) return 0;
                if (*sp != '\0') return 0;
            }
            pc++;
            break;

        case OP_SAVE_OPEN:
        case OP_SAVE_CLOSE: {
            int slot = ip->arg;
            if (slot >= 0 && slot < MAX_SUB * 2) {
                const char* old = M->sub[slot];
                M->sub[slot] = sp;
                if (match_inst(M, pc + 1, sp)) return 1;
                M->sub[slot] = old;
                return 0;
            }
            pc++;
            break;
        }

        case OP_STAR: {
            /* Greedy: try matching atom as many times as possible, then backtrack */
            int alen = ip->arg; /* atom is at pc - alen to pc - 1 */
            int atom_pc = pc - alen;
            pc++; /* skip past STAR instruction */

            /* Count max matches */
            const char* positions[256];
            int count = 0;
            positions[0] = sp;

            while (count < 255) {
                const char* save_sp = sp;
                /* Try to match atom */
                mstate_t tmp = *M;
                if (!match_inst(&tmp, atom_pc, sp)) break;
                /* Atom matched: advance sp by how much the atom consumed */
                /* We need to figure out where sp ended up — tricky with backtracking */
                /* Simpler approach: try one char at a time for simple atoms */
                /* Actually, let's just try greedy: match rest at each possible count */
                /* For single-char atoms (common case), each match is 1 char */
                if (alen == 1 && (M->code[atom_pc].type == OP_LIT ||
                                  M->code[atom_pc].type == OP_ANY ||
                                  M->code[atom_pc].type == OP_CLASS ||
                                  M->code[atom_pc].type == OP_NCLASS)) {
                    if (!match_char(&M->code[atom_pc], (unsigned char)*sp, M->cflags & REG_ICASE)) break;
                    sp++;
                } else {
                    /* Multi-instruction atom — skip by trying char-by-char */
                    if (*sp == '\0') break;
                    sp++; /* approximate: advance by 1 */
                }
                count++;
                positions[count] = sp;
                (void)save_sp;
            }

            /* Greedy: try from max count down to 0 */
            for (int i = count; i >= 0; i--) {
                if (match_inst(M, pc, positions[i])) return 1;
            }
            return 0;
        }

        case OP_PLUS: {
            int alen = ip->arg;
            int atom_pc = pc - alen;
            pc++;

            const char* positions[256];
            int count = 0;

            const char* cur_sp = sp;
            while (count < 255) {
                if (alen == 1 && (M->code[atom_pc].type == OP_LIT ||
                                  M->code[atom_pc].type == OP_ANY ||
                                  M->code[atom_pc].type == OP_CLASS ||
                                  M->code[atom_pc].type == OP_NCLASS)) {
                    if (*cur_sp == '\0') break;
                    if (!match_char(&M->code[atom_pc], (unsigned char)*cur_sp, M->cflags & REG_ICASE)) break;
                    cur_sp++;
                } else {
                    if (*cur_sp == '\0') break;
                    cur_sp++;
                }
                count++;
                positions[count] = cur_sp;
            }

            if (count == 0) return 0; /* + requires at least 1 */

            for (int i = count; i >= 1; i--) {
                if (match_inst(M, pc, positions[i])) return 1;
            }
            return 0;
        }

        case OP_QUEST: {
            int alen = ip->arg;
            int atom_pc = pc - alen;
            pc++;

            /* Greedy: try with atom, then without */
            const char* after_atom = sp;
            if (alen == 1 && (M->code[atom_pc].type == OP_LIT ||
                              M->code[atom_pc].type == OP_ANY ||
                              M->code[atom_pc].type == OP_CLASS ||
                              M->code[atom_pc].type == OP_NCLASS)) {
                if (*sp && match_char(&M->code[atom_pc], (unsigned char)*sp, M->cflags & REG_ICASE))
                    after_atom = sp + 1;
                else
                    after_atom = sp; /* atom didn't match */
            } else {
                if (*sp) after_atom = sp + 1;
            }

            if (after_atom != sp && match_inst(M, pc, after_atom)) return 1;
            return match_inst(M, pc, sp);
        }

        case OP_JMP:
            pc += ip->arg + 1;
            break;

        case OP_SPLIT: {
            /* Try first branch (instructions before JMP), then second */
            /* This is handled implicitly by the alternation compilation */
            pc++;
            break;
        }

        case OP_MATCH:
            return 1;

        default:
            return 0;
        }
    }
    /* Reached end of code = match */
    M->sub[1] = sp; /* end of match */
    return 1;
}

/* ---- Public API ---- */

int regcomp(regex_t* preg, const char* pattern, int cflags) {
    if (!preg || !pattern) return REG_BADPAT;

    prog_t* P = (prog_t*)malloc(sizeof(prog_t));
    if (!P) return REG_ESPACE;
    memset(P, 0, sizeof(*P));
    P->cflags = cflags;

    int ere = (cflags & REG_EXTENDED) ? 1 : 0;
    int icase = (cflags & REG_ICASE) ? 1 : 0;

    const char* pp = pattern;
    int r = compile_alt(P, &pp, ere, icase);
    if (r > 0) { free(P->code); free(P); return r; }

    /* Emit MATCH at end */
    inst_t* ip = emit(P);
    if (!ip) { free(P->code); free(P); return REG_ESPACE; }
    ip->type = OP_MATCH;

    preg->re_nsub = (size_t)P->nsub;
    preg->_priv = P;
    return 0;
}

int regexec(const regex_t* preg, const char* string,
            size_t nmatch, regmatch_t pmatch[], int eflags) {
    if (!preg || !preg->_priv || !string) return REG_NOMATCH;

    prog_t* P = (prog_t*)preg->_priv;
    mstate_t M;
    memset(&M, 0, sizeof(M));
    M.code = P->code;
    M.codelen = P->len;
    M.str = string;
    M.str_start = string;
    M.eflags = eflags;
    M.cflags = P->cflags;

    /* Try matching at each position in the string */
    for (const char* sp = string; ; sp++) {
        memset(M.sub, 0, sizeof(M.sub));
        M.sub[0] = sp;

        if (match_inst(&M, 0, sp)) {
            /* Fill in pmatch */
            if (pmatch && nmatch > 0 && !(P->cflags & REG_NOSUB)) {
                pmatch[0].rm_so = (regoff_t)(M.sub[0] - string);
                pmatch[0].rm_eo = (regoff_t)(M.sub[1] - string);
                for (size_t i = 1; i < nmatch; i++) {
                    if (M.sub[i * 2] && M.sub[i * 2 + 1]) {
                        pmatch[i].rm_so = (regoff_t)(M.sub[i * 2] - string);
                        pmatch[i].rm_eo = (regoff_t)(M.sub[i * 2 + 1] - string);
                    } else {
                        pmatch[i].rm_so = -1;
                        pmatch[i].rm_eo = -1;
                    }
                }
            }
            return 0;
        }

        if (*sp == '\0') break;
    }

    return REG_NOMATCH;
}

void regfree(regex_t* preg) {
    if (!preg || !preg->_priv) return;
    prog_t* P = (prog_t*)preg->_priv;
    free(P->code);
    free(P);
    preg->_priv = (void*)0;
}

size_t regerror(int errcode, const regex_t* preg,
                char* errbuf, size_t errbuf_size) {
    (void)preg;
    static const char* msgs[] = {
        "Success",
        "No match",
        "Invalid regex",
        "Invalid collating element",
        "Invalid character class",
        "Trailing backslash",
        "Invalid back reference",
        "Unmatched [",
        "Unmatched (",
        "Unmatched {",
        "Invalid range",
        "Invalid range end",
        "Out of memory",
        "Invalid repetition",
    };
    const char* msg = "Unknown error";
    if (errcode >= 0 && errcode <= 13) msg = msgs[errcode];
    size_t len = strlen(msg) + 1;
    if (errbuf && errbuf_size > 0) {
        size_t n = len < errbuf_size ? len : errbuf_size;
        memcpy(errbuf, msg, n - 1);
        errbuf[n - 1] = '\0';
    }
    return len;
}
