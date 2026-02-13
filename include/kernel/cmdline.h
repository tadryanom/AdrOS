#ifndef KERNEL_CMDLINE_H
#define KERNEL_CMDLINE_H

#include <stddef.h>

/*
 * Linux-like kernel command line parser.
 *
 * The bootloader (GRUB) passes a command line string like:
 *   "/boot/adros-x86.bin init=/bin/init.elf quiet -- custom_arg"
 *
 * Parsing rules:
 *   1. First token is the kernel path — skipped.
 *   2. Recognized kernel params (e.g. "init=", "root=", "quiet") are
 *      consumed by the kernel.
 *   3. The separator "--" marks the boundary: everything after it is
 *      forwarded to the init process untouched.
 *   4. Before "--": unrecognized "key=value" tokens become init
 *      environment variables (envp[]).
 *   5. Before "--": unrecognized plain tokens (no '=' or '.') become
 *      init command-line arguments (argv[]).
 *   6. After "--": "key=value" → envp[], plain → argv[].
 */

#define CMDLINE_MAX         512
#define CMDLINE_MAX_ARGS    16
#define CMDLINE_MAX_ENVS    16

/* Call once during early init to parse the raw cmdline string. */
void cmdline_parse(const char* raw);

/* ---- Kernel parameter accessors ---- */

/* Return value of a "key=value" kernel param, or NULL if absent. */
const char* cmdline_get(const char* key);

/* Return 1 if a kernel flag (no value) is present. */
int         cmdline_has(const char* flag);

/* Return the init binary path (from "init=" or default). */
const char* cmdline_init_path(void);

/* Return the full raw cmdline (for /proc/cmdline). */
const char* cmdline_raw(void);

/* ---- Init process argv / envp ---- */

/* Return NULL-terminated argv array for init. argc_out receives count. */
const char* const* cmdline_init_argv(int* argc_out);

/* Return NULL-terminated envp array for init. envc_out receives count. */
const char* const* cmdline_init_envp(int* envc_out);

#endif
