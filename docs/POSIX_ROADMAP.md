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
- [~] VFS is currently “single-root tree” (no mounts, no multiple fs)
- [ ] Writable filesystem support

### Userspace bring-up
- [x] ELF32 userspace loader from VFS (`/bin/init.elf`)
- [~] Process model is minimal (no fork/exec/wait lifecycle)
- [x] `int 0x80` syscall entry (x86)

### Syscalls (current)
- [x] `write(fd=1/2)`
- [x] `exit()` (currently halts in kernel)
- [x] `getpid()` (placeholder)
- [x] `open()` (read-only)
- [x] `read()` (files + stdin)
- [x] `close()`

### FD layer
- [x] Per-process fd table (fd allocation starts at 3)
- [x] File read offset tracking
- [~] No `dup/dup2`, no `pipe`, no `lseek`

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
- [ ] Introduce parent/child relationship tracking
- [ ] Track exit status per process
- [ ] Transition to `PROCESS_ZOMBIE` on exit
- [ ] Reap zombie processes and free resources

### `exit()` cleanup
- [ ] Close all open file descriptors for the process
- [ ] Release process memory resources (as applicable in current model)
- [ ] Remove process from run queues safely

### `waitpid()` syscall
- [ ] Add syscall number + userland wrapper
- [ ] `waitpid(-1, ...)` wait for any child
- [ ] `waitpid(pid, ...)` wait for specific child
- [ ] Non-blocking mode (optional early): `WNOHANG`
- [ ] Return semantics consistent with POSIX (pid on success, -1 on error)

### Tests
- [ ] Userspace test: parent spawns child, child exits, parent waits, validates status
- [ ] Regression: ensure keyboard/TTY still works

---

## 2) Milestone A2 — Address spaces per process

Goal: move from a shared address space to per-process virtual memory, required for real isolation and POSIX process semantics.

### Core VM changes
- [ ] Per-process page directory / page tables
- [ ] Context switch also switches address space
- [ ] Kernel mapped in all address spaces
- [ ] User/kernel separation rules enforced

### Syscall/uaccess hardening
- [ ] Ensure `user_range_ok` is robust across per-process mappings
- [ ] Page-fault handling for invalid user pointers (deliver `SIGSEGV` later)

### Userspace loader
- [ ] ELF loader targets the new process address space
- [ ] User stack per process

### Tests
- [ ] Smoke: boot + run `/bin/init.elf`
- [ ] Two-process test: verify isolation (write to memory in one does not affect other)

---

## 3) Milestone B1 — POSIX-ish file API basics (`lseek`, `stat/fstat`)

Goal: unlock standard libc-style IO patterns.

### Syscalls
- [ ] `lseek(fd, off, whence)`
- [ ] `stat(path, struct stat*)`
- [ ] `fstat(fd, struct stat*)`

### Kernel data model
- [ ] Define minimal `struct stat` ABI (mode/type/size/inode)
- [ ] Map InitRD node metadata to `stat`

### Error model
- [ ] Start introducing `errno`-style error returns (strategy decision: negative errno vs -1 + errno)

### Tests
- [ ] Userspace test: open -> fstat -> read -> lseek -> read

---

## 4) Milestone C1 — Mounts + `tmpfs` (writable)

Goal: get a writable filesystem (even if volatile) and a real VFS layout.

### VFS mounts
- [ ] Mount table support
- [ ] `vfs_lookup` resolves across mounts
- [ ] Mount InitRD at `/` or at `/initrd` (decision)

### `tmpfs`
- [ ] In-memory inode/dentry model
- [ ] Create/unlink
- [ ] Read/write
- [ ] Directories

### Devices (minimum Unix feel)
- [ ] `/dev` mount
- [ ] `/dev/tty`
- [ ] `/dev/null`

### Tests
- [ ] Userspace test: create file in tmpfs, write, read back

---

## 5) Later milestones (not started)

### Process / POSIX expansion
- [ ] `fork()`
- [ ] `execve()`
- [ ] `getppid()`
- [ ] Signals + basic job control

### Pipes + IO multiplexing
- [ ] `pipe()`
- [ ] `dup/dup2`
- [ ] `select/poll`

### TTY advanced
- [ ] termios flags (canonical/raw/echo)
- [ ] controlling terminal, sessions, pgrp
- [ ] PTY for userland shells
