# AdrOS POSIX Roadmap (Checklist)

This document tracks **what is already implemented** versus **what is missing** to reach a practical Unix-like system with full POSIX compatibility.

Notes:
- This is intentionally pragmatic: items are ordered to unlock userland capabilities quickly.
- Checkboxes reflect the current state of the `master` branch.

## Status Legend
- `[x]` implemented (works end-to-end, smoke-tested)
- `[~]` partial (exists but incomplete/limited)
- `[ ]` not implemented

---

## 1. Syscalls — File I/O

| Syscall | Status | Notes |
|---------|--------|-------|
| `open` | [x] | Supports `O_CREAT`, `O_TRUNC`; works on diskfs, devfs, tmpfs, overlayfs |
| `openat` | [x] | `AT_FDCWD` supported; other dirfd values return `ENOSYS` |
| `read` | [x] | Files, pipes, TTY, PTY; `O_NONBLOCK` returns `EAGAIN` |
| `write` | [x] | Files, pipes, TTY, PTY; `O_NONBLOCK` returns `EAGAIN` |
| `close` | [x] | Refcounted file objects |
| `lseek` | [x] | `SEEK_SET`, `SEEK_CUR`, `SEEK_END` |
| `stat` | [x] | Minimal `struct stat` (mode/type/size/inode) |
| `fstat` | [x] | |
| `fstatat` | [x] | `AT_FDCWD` supported |
| `dup` | [x] | |
| `dup2` | [x] | |
| `dup3` | [x] | Flags parameter (currently only `flags=0` accepted) |
| `pipe` | [x] | In-kernel ring buffer |
| `pipe2` | [x] | Supports `O_NONBLOCK` flag |
| `select` | [x] | Minimal (pipes, TTY) |
| `poll` | [x] | Minimal (pipes, TTY, `/dev/null`) |
| `ioctl` | [x] | `TCGETS`, `TCSETS`, `TIOCGPGRP`, `TIOCSPGRP` |
| `fcntl` | [x] | `F_GETFL`, `F_SETFL` (for `O_NONBLOCK`) |
| `getdents` | [x] | Generic across all VFS (diskfs, tmpfs, devfs, overlayfs) |
| `pread`/`pwrite` | [ ] | |
| `readv`/`writev` | [ ] | |
| `truncate`/`ftruncate` | [ ] | |
| `fsync`/`fdatasync` | [ ] | |

## 2. Syscalls — Directory & Path Operations

| Syscall | Status | Notes |
|---------|--------|-------|
| `mkdir` | [x] | diskfs |
| `rmdir` | [x] | diskfs; checks directory is empty (`ENOTEMPTY`) |
| `unlink` | [x] | diskfs; returns `EISDIR` for directories |
| `unlinkat` | [x] | `AT_FDCWD` supported |
| `rename` | [x] | diskfs; handles same-type overwrite |
| `chdir` | [x] | Per-process `cwd` |
| `getcwd` | [x] | |
| `link` | [ ] | Hard links |
| `symlink` | [ ] | Symbolic links |
| `readlink` | [ ] | |
| `access` | [ ] | Permission checks |
| `umask` | [ ] | |
| `realpath` | [ ] | Userland (needs libc) |

## 3. Syscalls — Process Management

| Syscall | Status | Notes |
|---------|--------|-------|
| `fork` | [x] | Full COW not implemented; copies address space |
| `execve` | [~] | Loads ELF from VFS; minimal argv/envp; no `$PATH` search |
| `exit` / `_exit` | [x] | Closes FDs, marks zombie, notifies parent |
| `waitpid` | [x] | `-1` (any child), specific pid, `WNOHANG` |
| `getpid` | [x] | |
| `getppid` | [x] | |
| `setsid` | [x] | |
| `setpgid` | [x] | |
| `getpgrp` | [x] | |
| `getuid`/`getgid`/`geteuid`/`getegid` | [ ] | No user/group model yet |
| `setuid`/`setgid` | [ ] | |
| `brk`/`sbrk` | [ ] | Heap management |
| `mmap`/`munmap` | [ ] | Memory-mapped I/O |
| `clone` | [ ] | Thread creation |
| `nanosleep`/`sleep` | [ ] | |
| `alarm` | [ ] | |
| `times`/`getrusage` | [ ] | |

## 4. Syscalls — Signals

| Syscall | Status | Notes |
|---------|--------|-------|
| `sigaction` | [x] | Installs handlers; `sa_flags` minimal |
| `sigprocmask` | [x] | Block/unblock signals |
| `kill` | [x] | Send signal to process/group |
| `sigreturn` | [x] | Trampoline-based return from signal handlers |
| `raise` | [ ] | Userland (needs libc) |
| `sigpending` | [ ] | |
| `sigsuspend` | [ ] | |
| `sigqueue` | [ ] | |
| `sigaltstack` | [ ] | Alternate signal stack |
| Signal defaults | [~] | `SIGKILL`/`SIGSEGV`/`SIGUSR1` handled; many signals missing default actions |

## 5. File Descriptor Layer

| Feature | Status | Notes |
|---------|--------|-------|
| Per-process fd table | [x] | Up to `PROCESS_MAX_FILES` entries |
| Refcounted file objects | [x] | Shared across `dup`/`fork` |
| File offset tracking | [x] | |
| `O_NONBLOCK` | [x] | Pipes, TTY, PTY via `fcntl` or `pipe2` |
| `O_CLOEXEC` | [ ] | Close-on-exec flag |
| `O_APPEND` | [ ] | |
| `FD_CLOEXEC` via `fcntl` | [ ] | |
| File locking (`flock`/`fcntl`) | [ ] | |

## 6. Filesystem / VFS

| Feature | Status | Notes |
|---------|--------|-------|
| VFS mount table | [x] | Up to 8 mounts |
| `vfs_lookup` path resolution | [x] | Absolute + relative (via `cwd`) |
| `fs_node_t` with `read`/`write`/`finddir`/`readdir` | [x] | |
| `struct vfs_dirent` (generic) | [x] | Unified format across all FS |
| **tmpfs** | [x] | In-memory; dirs + files; `readdir` |
| **overlayfs** | [x] | Copy-up; `readdir` delegates to upper/lower |
| **devfs** | [x] | `/dev/null`, `/dev/tty`, `/dev/ptmx`, `/dev/pts/0`; `readdir` |
| **diskfs** (on-disk) | [x] | Hierarchical inodes; `open`/`read`/`write`/`stat`/`mkdir`/`unlink`/`rmdir`/`rename`/`getdents` |
| **persistfs** | [x] | Minimal persistence at `/persist` |
| Permissions (`uid`/`gid`/mode) | [ ] | No permission model |
| Hard links | [ ] | |
| Symbolic links | [ ] | |
| `/proc` filesystem | [ ] | |
| ext2 / FAT support | [ ] | |

## 7. TTY / PTY

| Feature | Status | Notes |
|---------|--------|-------|
| Canonical input (line-buffered) | [x] | |
| Echo + backspace | [x] | |
| Blocking reads + wait queue | [x] | |
| `TCGETS`/`TCSETS` | [x] | Minimal termios |
| `TIOCGPGRP`/`TIOCSPGRP` | [x] | |
| Job control (`SIGTTIN`/`SIGTTOU`) | [x] | Background pgrp enforcement |
| `isatty` (via `ioctl TCGETS`) | [x] | |
| PTY master/slave | [x] | `/dev/ptmx` + `/dev/pts/0` |
| Non-blocking PTY I/O | [x] | |
| Raw mode (non-canonical) | [ ] | |
| VMIN/VTIME | [ ] | |
| Signal characters (Ctrl+C → `SIGINT`, etc.) | [ ] | |
| Multiple PTY pairs | [ ] | Only 1 pair currently |
| Window size (`TIOCGWINSZ`/`TIOCSWINSZ`) | [ ] | |

## 8. Memory Management

| Feature | Status | Notes |
|---------|--------|-------|
| PMM (bitmap allocator) | [x] | |
| VMM (x86 paging) | [x] | Higher-half kernel |
| Per-process address spaces | [x] | Page directory per process |
| Kernel heap (`kmalloc`/`kfree`) | [x] | 10MB heap |
| W^X for user ELFs | [x] | Text segments read-only after load |
| `brk`/`sbrk` | [ ] | |
| `mmap`/`munmap` | [ ] | |
| Copy-on-write (COW) fork | [ ] | Currently full-copy |
| PAE + NX bit | [ ] | |
| Guard pages | [ ] | |
| ASLR | [ ] | |

## 9. Drivers & Hardware

| Feature | Status | Notes |
|---------|--------|-------|
| UART serial console | [x] | |
| VGA text console (x86) | [x] | |
| PS/2 keyboard | [x] | |
| PIT timer | [x] | |
| ATA PIO (IDE) | [x] | Primary master |
| RTC (real-time clock) | [ ] | |
| PCI enumeration | [ ] | |
| Framebuffer / VESA | [ ] | |
| Network (e1000/virtio-net) | [ ] | |
| Virtio-blk | [ ] | |

## 10. Userland

| Feature | Status | Notes |
|---------|--------|-------|
| ELF32 loader | [x] | |
| `/bin/init.elf` (smoke tests) | [x] | Comprehensive test suite |
| `/bin/echo.elf` | [x] | Minimal argv/envp test |
| Minimal libc | [ ] | No libc; userland uses raw syscall wrappers |
| Shell (`sh`) | [ ] | |
| Core utilities (`ls`, `cat`, `cp`, `mv`, `rm`, `mkdir`) | [ ] | |
| Dynamic linking | [ ] | |
| `$PATH` search in `execve` | [ ] | |

## 11. Networking (future)

| Feature | Status | Notes |
|---------|--------|-------|
| `socket` | [ ] | |
| `bind`/`listen`/`accept` | [ ] | |
| `connect`/`send`/`recv` | [ ] | |
| TCP/IP stack | [ ] | |
| UDP | [ ] | |
| DNS resolver | [ ] | |
| `/etc/hosts` | [ ] | |
| `getaddrinfo` | [ ] | Userland (needs libc) |

---

## Priority Roadmap (next steps)

### Near-term (unlock a usable shell)
1. **Minimal libc** — `printf`, `malloc`/`free`, `string.h`, `stdio.h` wrappers
2. **Shell** — `sh`-compatible; needs `fork`+`execve`+`waitpid`+`pipe`+`dup2`+`chdir` (all implemented)
3. **Core utilities** — `ls` (uses `getdents`), `cat`, `echo`, `mkdir`, `rm`, `mv`, `cp`
4. **Signal characters** — Ctrl+C → `SIGINT`, Ctrl+Z → `SIGTSTP`, Ctrl+D → EOF
5. **Raw TTY mode** — needed for interactive editors and proper shell line editing

### Medium-term (real POSIX compliance)
6. **`brk`/`sbrk`** — userland heap
7. **`mmap`/`munmap`** — memory-mapped files, shared memory
8. **Permissions** — `uid`/`gid`, mode bits, `chmod`, `chown`, `access`, `umask`
9. **`O_CLOEXEC`** — close-on-exec for fd hygiene
10. **`/proc`** — process information filesystem
11. **Hard/symbolic links** — `link`, `symlink`, `readlink`

### Long-term (full Unix experience)
12. **Networking** — socket API, TCP/IP stack
13. **Multi-arch bring-up** — ARM/RISC-V functional kernels
14. **COW fork + demand paging**
15. **Threads** (`clone`/`pthread`)
16. **Dynamic linking** (`ld.so`)
17. **ext2/FAT** filesystem support
