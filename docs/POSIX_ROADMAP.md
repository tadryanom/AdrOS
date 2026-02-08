# AdrOS POSIX Roadmap (Checklist)

This document tracks **what is already implemented** versus **what is missing** to reach a practical Unix-like system with increasing POSIX compatibility.

Notes:
- This is intentionally pragmatic: items are ordered to unlock userland capabilities quickly.
- Checkboxes reflect the current state of the `master` branch.

## Status Legend
- `[x]` implemented (works end-to-end)
- `[~]` partial (exists but incomplete/limited)
- `[ ]` not implemented

---

## 0) Current Baseline (Already in tree)

### Boot / platform / core kernel
- [x] x86 (i386) boot via GRUB2 Multiboot2
- [x] Higher-half kernel mapping
- [x] IDT + IRQ enable
- [x] Basic scheduler / kernel threads
- [x] Timer tick
- [x] Kernel heap (`kmalloc`/`kfree`)
- [~] Multi-arch stubs (ARM/RISC-V/MIPS) (not functionally brought up)

### InitRD + filesystem basics
- [x] InitRD format: TAR/USTAR
- [x] InitRD directory tree support
- [x] `fs_node_t` abstraction with `read/finddir` for InitRD nodes
- [x] `vfs_lookup()` absolute path resolver
- [x] VFS mount table support (`vfs_mount`)
- [x] Writable filesystem support (`tmpfs`)
- [x] `overlayfs` (copy-up overlay for root)

### Userspace bring-up
- [x] ELF32 userspace loader from VFS (`/bin/init.elf`)
- [~] Process model is minimal, but Unix-like primitives exist (fork/exec/wait)
- [x] `int 0x80` syscall entry (x86)

### Syscalls (current)
- [x] `write(fd=1/2)`
- [x] `exit()` (closes FDs, marks zombie, notifies parent)
- [~] `getpid()` (minimal)
- [x] `open()` (read-only)
- [x] `read()` (files + stdin)
- [x] `close()`
- [x] `waitpid()`
- [x] `waitpid(..., WNOHANG)`
- [x] `lseek()`
- [x] `stat()` / `fstat()`
- [x] `dup()` / `dup2()`
- [x] `pipe()`
- [x] `fork()`
- [~] `execve()` (loads ELF from InitRD; minimal argv/envp)
- [x] `getppid()`

### FD layer
- [x] Per-process fd table (fd allocation starts at 3)
- [x] File read offset tracking
- [x] `dup/dup2` with refcounted file objects
- [x] `pipe()` with in-kernel ring buffer endpoints
- [x] `lseek()`

### TTY
- [x] TTY canonical input (line-buffered until `\n`)
- [x] Echo + backspace handling
- [x] Blocking reads (process `BLOCKED`) + wait queue (multiple waiters)
- [x] `fd=0` wired to `tty_read`, `fd=1/2` wired to `tty_write`
- [ ] Termios-like configuration
- [ ] PTY

---

## 1) Milestone A1 — Process lifecycle: `waitpid` + cleanup on `exit`

Goal: make process termination and waiting work reliably; unblock shells and service managers.

### Kernel process lifecycle
- [x] Introduce parent/child relationship tracking
- [x] Track exit status per process
- [x] Transition to `PROCESS_ZOMBIE` on exit
- [x] Reap zombie processes and free resources

### `exit()` cleanup
- [x] Close all open file descriptors for the process
- [x] Release process memory resources (kernel stack + user addr_space when reaped)
- [~] Remove process from run queues safely (best-effort; continues improving)

### `waitpid()` syscall
- [x] Add syscall number + userland wrapper
- [x] `waitpid(-1, ...)` wait for any child
- [x] `waitpid(pid, ...)` wait for specific child
- [x] Non-blocking mode (optional early): `WNOHANG`
- [~] Return semantics consistent with POSIX (pid on success, -1 on error)

### Tests
- [x] Userspace test: parent forks children, children exit, parent waits, validates status
- [ ] Regression: ensure keyboard/TTY still works

---

## 2) Milestone A2 — Address spaces per process

Goal: move from a shared address space to per-process virtual memory, required for real isolation and POSIX process semantics.

### Core VM changes
- [x] Per-process page directory / page tables
- [x] Context switch also switches address space
- [x] Kernel mapped in all address spaces
- [~] User/kernel separation rules enforced (uaccess checks + no user mappings in kernel range)

### Syscall/uaccess hardening
- [~] Ensure `user_range_ok` is robust across per-process mappings
- [x] `copy_to_user` requires writable user mappings (x86)
- [ ] Page-fault handling for invalid user pointers (deliver `SIGSEGV` later)

### Userspace loader
- [x] ELF loader targets the new process address space
- [x] User stack per process

### Tests
- [x] Smoke: boot + run `/bin/init.elf`
- [ ] Two-process test: verify isolation (write to memory in one does not affect other)

---

## 3) Milestone B1 — POSIX-ish file API basics (`lseek`, `stat/fstat`)

Goal: unlock standard libc-style IO patterns.

### Syscalls
- [x] `lseek(fd, off, whence)`
- [x] `stat(path, struct stat*)`
- [x] `fstat(fd, struct stat*)`

### Kernel data model
- [x] Define minimal `struct stat` ABI (mode/type/size/inode)
- [x] Map InitRD node metadata to `stat`

### Error model
- [x] Negative errno returns in kernel/syscalls (`-errno`)
- [ ] Userspace `errno` + libc-style wrappers (`-1` + `errno`)

### Tests
- [x] Userspace test: open -> fstat -> read -> lseek -> read

---

## 4) Milestone C1 — Mounts + `tmpfs` (writable)

Goal: get a writable filesystem (even if volatile) and a real VFS layout.

### VFS mounts
- [x] Mount table support
- [x] `vfs_lookup` resolves across mounts
- [ ] Mount InitRD at `/` or at `/initrd` (decision)

### `tmpfs`
- [x] In-memory inode/dentry model
- [~] Create/unlink (limited)
- [x] Read/write
- [x] Directories

### Devices (minimum Unix feel)
- [x] `/dev` mount
- [x] `/dev/tty`
- [x] `/dev/null`

### Tests
- [x] Userspace test: create file in tmpfs, write, read back

---

## 5) Later milestones (not started)

### Process / POSIX expansion
- [x] `fork()`
- [~] `execve()`
- [x] `getppid()`
- [ ] Signals + basic job control

### Pipes + IO multiplexing
- [x] `pipe()`
- [x] `dup/dup2`
- [ ] `select/poll`

### TTY advanced
- [ ] termios flags (canonical/raw/echo)
- [ ] controlling terminal, sessions, pgrp
- [ ] PTY for userland shells
