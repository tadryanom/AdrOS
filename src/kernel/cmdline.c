#include "kernel/cmdline.h"
#include "utils.h"
#include "console.h"

#include <stdint.h>

/* ---- Static storage ---- */

static char raw_copy[CMDLINE_MAX];   /* pristine copy for /proc/cmdline */
static char tok_copy[CMDLINE_MAX];   /* tokenized copy — pointers live here */

/* Kernel-recognized "key=value" parameters */
#define KPARAM_MAX 16

struct kparam {
    const char* key;
    const char* value;   /* points into tok_copy */
};

static struct kparam kparams[KPARAM_MAX];
static int           kparam_count;

/* Kernel-recognized flags (no value) */
#define KFLAG_MAX 16

static const char*   kflags[KFLAG_MAX];
static int           kflag_count;

/* Init argv / envp (pointers into raw_copy) */
static const char*   init_argv[CMDLINE_MAX_ARGS + 1]; /* +1 for NULL */
static int           init_argc;

static const char*   init_envp[CMDLINE_MAX_ENVS + 1];
static int           init_envc;

static const char*   init_path_val;

/* ---- Tables of recognized kernel tokens ---- */

static const char* const known_kv_keys[] = {
    "init", "root", "console", "loglevel", NULL
};

static const char* const known_flags[] = {
    "quiet", "ring3", "nokaslr", "single", "noapic", "nosmp", NULL
};

static int is_known_kv_key(const char* key, size_t keylen) {
    for (int i = 0; known_kv_keys[i]; i++) {
        if (strlen(known_kv_keys[i]) == keylen &&
            memcmp(known_kv_keys[i], key, keylen) == 0)
            return 1;
    }
    return 0;
}

static int is_known_flag(const char* tok) {
    for (int i = 0; known_flags[i]; i++) {
        if (strcmp(known_flags[i], tok) == 0)
            return 1;
    }
    return 0;
}

/* ---- Helpers ---- */

static int has_char(const char* s, char c) {
    while (*s) { if (*s == c) return 1; s++; }
    return 0;
}

/* ---- Parser ---- */

void cmdline_parse(const char* raw) {
    kparam_count = 0;
    kflag_count = 0;
    init_argc = 0;
    init_envc = 0;
    init_path_val = "/bin/init.elf";

    if (!raw) {
        raw_copy[0] = '\0';
        init_argv[0] = NULL;
        init_envp[0] = NULL;
        return;
    }

    /* Copy raw cmdline (preserve original for /proc/cmdline) */
    strncpy(raw_copy, raw, CMDLINE_MAX - 1);
    raw_copy[CMDLINE_MAX - 1] = '\0';

    /* Tokenize a second copy in-place — all pointers will live here */
    strncpy(tok_copy, raw, CMDLINE_MAX - 1);
    tok_copy[CMDLINE_MAX - 1] = '\0';

    char* tokens[128];
    int ntokens = 0;

    /* Tokenize by whitespace */
    char* p = tok_copy;
    while (*p && ntokens < 128) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        tokens[ntokens++] = p;
        while (*p && *p != ' ' && *p != '\t') p++;
        if (*p) { *p = '\0'; p++; }
    }

    if (ntokens == 0) {
        init_argv[0] = NULL;
        init_envp[0] = NULL;
        return;
    }

    /* Token 0 may be the kernel path (e.g. "/boot/adros-x86.bin") —
     * skip it only if it looks like a path.  GRUB2 with Multiboot2
     * does NOT always include the kernel path in the cmdline string;
     * it may pass only the arguments (e.g. "ring3" or "init=/bin/sh"). */
    int start_idx = 0;
    if (ntokens > 0 && tokens[0][0] == '/') {
        start_idx = 1;
    }

    int after_separator = 0;

    for (int i = start_idx; i < ntokens; i++) {
        const char* tok = tokens[i];

        /* Check for "--" separator */
        if (tok[0] == '-' && tok[1] == '-' && tok[2] == '\0') {
            after_separator = 1;
            continue;
        }

        /* Find '=' if present */
        const char* eq = NULL;
        for (const char* c = tok; *c; c++) {
            if (*c == '=') { eq = c; break; }
        }

        if (!after_separator) {
            /* Before "--": kernel tries to claim the token */
            if (eq) {
                /* "key=value" form */
                size_t keylen = (size_t)(eq - tok);
                if (is_known_kv_key(tok, keylen)) {
                    if (kparam_count < KPARAM_MAX) {
                        /* Split "key=value" in tok_copy at '=' */
                        char* eq_ptr = (char*)(uintptr_t)eq;
                        *eq_ptr = '\0';
                        kparams[kparam_count].key = tok;
                        kparams[kparam_count].value = eq_ptr + 1;
                        kparam_count++;
                    }
                } else {
                    /* Unrecognized key=value → init envp */
                    if (init_envc < CMDLINE_MAX_ENVS) {
                        init_envp[init_envc++] = tok;
                    }
                }
            } else {
                /* Plain token (no '=') */
                if (is_known_flag(tok)) {
                    if (kflag_count < KFLAG_MAX) {
                        kflags[kflag_count++] = tok;
                    }
                } else if (!has_char(tok, '.')) {
                    /* No '.' and not recognized → init argv */
                    if (init_argc < CMDLINE_MAX_ARGS) {
                        init_argv[init_argc++] = tok;
                    }
                }
                /* Tokens with '.' but no '=' (like module params) are
                 * silently ignored for now. */
            }
        } else {
            /* After "--": everything goes to init */
            if (eq) {
                if (init_envc < CMDLINE_MAX_ENVS) {
                    init_envp[init_envc++] = tok;
                }
            } else {
                if (init_argc < CMDLINE_MAX_ARGS) {
                    init_argv[init_argc++] = tok;
                }
            }
        }
    }

    init_argv[init_argc] = NULL;
    init_envp[init_envc] = NULL;

    /* Extract init= value if present */
    const char* init_val = cmdline_get("init");
    if (init_val && init_val[0] != '\0') {
        init_path_val = init_val;
    }

    /* Log parsed results */
    kprintf("[CMDLINE] \"%s\"\n", raw_copy);
    if (kparam_count > 0) {
        for (int i = 0; i < kparam_count; i++)
            kprintf("[CMDLINE]   kernel: %s=%s\n", kparams[i].key, kparams[i].value);
    }
    if (kflag_count > 0) {
        for (int i = 0; i < kflag_count; i++)
            kprintf("[CMDLINE]   flag: %s\n", kflags[i]);
    }
    if (init_argc > 0) {
        for (int i = 0; i < init_argc; i++)
            kprintf("[CMDLINE]   init argv[%d]: %s\n", i, init_argv[i]);
    }
    if (init_envc > 0) {
        for (int i = 0; i < init_envc; i++)
            kprintf("[CMDLINE]   init envp[%d]: %s\n", i, init_envp[i]);
    }
    kprintf("[CMDLINE]   init path: %s\n", init_path_val);
}

/* ---- Accessors ---- */

const char* cmdline_get(const char* key) {
    if (!key) return NULL;
    for (int i = 0; i < kparam_count; i++) {
        if (strcmp(kparams[i].key, key) == 0)
            return kparams[i].value;
    }
    return NULL;
}

int cmdline_has(const char* flag) {
    if (!flag) return 0;
    for (int i = 0; i < kflag_count; i++) {
        if (strcmp(kflags[i], flag) == 0)
            return 1;
    }
    return 0;
}

const char* cmdline_init_path(void) {
    return init_path_val;
}

const char* cmdline_raw(void) {
    return raw_copy;
}

const char* const* cmdline_init_argv(int* argc_out) {
    if (argc_out) *argc_out = init_argc;
    return init_argv;
}

const char* const* cmdline_init_envp(int* envc_out) {
    if (envc_out) *envc_out = init_envc;
    return init_envp;
}
