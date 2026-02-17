#ifndef ULIBC_SYS_WAIT_H
#define ULIBC_SYS_WAIT_H

/* AdrOS wait status encoding:
 *   Normal exit:  exit_status = code  (from _exit(code))
 *   Signal kill:  exit_status = 128 + sig
 *
 * We provide Linux-compatible macros that work with AdrOS's simple encoding.
 * Since AdrOS passes exit_status directly (not Linux-style packed), these
 * macros approximate the behavior:
 *   - exit code 0..127 → normal exit
 *   - exit code 128+   → killed by signal (128+sig)
 */

#define WNOHANG    1
#define WUNTRACED  2

#define WIFEXITED(s)    ((s) < 128)
#define WEXITSTATUS(s)  (s)
#define WIFSIGNALED(s)  ((s) >= 128)
#define WTERMSIG(s)     ((s) - 128)
#define WIFSTOPPED(s)   0
#define WSTOPSIG(s)     0

#endif
