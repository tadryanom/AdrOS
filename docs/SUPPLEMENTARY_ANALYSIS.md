# AdrOS — Supplementary Material Analysis & POSIX Gap Report

This document compares the **supplementary-material** reference code and suggestions
(from the AI monolog in `readme.txt` plus the `.c.txt`/`.S.txt` example files) with
the **current AdrOS implementation**, and assesses how close AdrOS is to being a
Unix-like, POSIX-compatible operating system.

---

## Part 1 — Subsystem-by-Subsystem Comparison

### 1.1 Physical Memory Manager (PMM)

| Aspect | Supplementary Suggestion | AdrOS Current State | Gap |
|--------|--------------------------|---------------------|-----|
| Bitmap allocator | ✅ Bitmap-based | ✅ Bitmap-based (`src/mm/pmm.c`) | None |
| Multiboot memory map parsing | ✅ Parse MMAP entries | ✅ Full Multiboot2 MMAP parsing, clamping, fallback | None |
| Kernel/module protection | ✅ Reserve kernel + initrd | ✅ Protects kernel (`_start`–`_end`), modules, low 1MB | None |
| Frame reference counting | ✅ `uint16_t frame_ref_count[]` for CoW | ✅ `pmm_incref`/`pmm_decref`/`pmm_get_refcount` | None |
| Contiguous block allocation | ✅ `pmm_alloc_blocks(count)` for DMA | ❌ Only single-frame `pmm_alloc_page()` | Needed for multi-page DMA |
| Atomic ref operations | ✅ `__sync_fetch_and_add` | ✅ File refcounts use `__sync_*` builtins | None |
| Spinlock protection | ✅ `spinlock_acquire(&pmm_lock)` | ✅ `pmm_lock` with `spin_lock_irqsave` | None |

**Summary:** AdrOS PMM is SMP-safe with spinlock protection and frame refcounting for CoW. Only missing contiguous block allocation.

---

### 1.2 Virtual Memory Manager (VMM)

| Aspect | Supplementary Suggestion | AdrOS Current State | Gap |
|--------|--------------------------|---------------------|-----|
| Higher-half kernel | ✅ 0xC0000000 | ✅ Identical | None |
| Recursive page directory | Mentioned but not detailed | ✅ PDE[1023] self-map, `x86_pd_recursive()` | AdrOS is ahead |
| Per-process address spaces | ✅ Clone kernel PD | ✅ `vmm_as_create_kernel_clone()`, `vmm_as_clone_user()` | None |
| W^X logical policy | ✅ `vmm_apply_wx_policy()` rejects RWX | ✅ ELF loader maps `.text` as RO after load via `vmm_protect_range()` | Partial — no policy function, but effect achieved |
| W^X hardware (NX bit) | ✅ PAE + NX via EFER MSR | ❌ 32-bit paging, no PAE, no NX | Long-term |
| CPUID feature detection | ✅ `cpu_get_features()` for PAE/NX | ✅ Full CPUID leaf 0/1/7/extended; SMEP/SMAP detection | None |
| SMEP | Not discussed | ✅ Enabled in CR4 if CPU supports | **AdrOS is ahead** |
| Copy-on-Write (CoW) | ✅ Full implementation | ✅ `vmm_as_clone_user_cow()` + `vmm_handle_cow_fault()` | None |
| `vmm_find_free_area()` | ✅ Scan user VA space for holes | ❌ Not implemented | Needed for `mmap` |
| `vmm_map_dma_buffer()` | ✅ Map phys into user VA | ❌ Not implemented | Needed for zero-copy I/O |
| TLB flush | ✅ `invlpg` + full flush | ✅ `invlpg()` per page | None |
| Spinlock on VMM ops | ✅ `vmm_kernel_lock` | ❌ No lock | Needed for SMP |

**Summary:** AdrOS VMM is functional and well-designed with CoW fork, recursive mapping, and SMEP. Missing hardware NX (requires PAE migration) and free-area search for `mmap`.

---

### 1.3 Kernel Heap

| Aspect | Supplementary Suggestion | AdrOS Current State | Gap |
|--------|--------------------------|---------------------|-----|
| Doubly-linked free list | Mentioned | ✅ `heap.c` with `HEAP_MAGIC` validation | None |
| Coalescing | Mentioned | ✅ Forward + backward coalesce (fixed in previous session) | None |
| Spinlock | ✅ Required | ✅ `heap_lock` spinlock present | None |
| Slab allocator | ✅ `slab_cache_t` for fixed-size objects | ✅ `slab_cache_init`/`slab_alloc`/`slab_free` with spinlock | None |
| Dynamic growth | Not discussed | ✅ Heap grows from 10MB up to 64MB on demand | **AdrOS is ahead** |

**Summary:** Heap works correctly with dynamic growth and slab allocator for fixed-size objects.

---

### 1.4 Process Scheduler

| Aspect | Supplementary Suggestion | AdrOS Current State | Gap |
|--------|--------------------------|---------------------|-----|
| Process states | ✅ READY/RUNNING/SLEEPING/ZOMBIE | ✅ READY/RUNNING/ZOMBIE/BLOCKED/SLEEPING | AdrOS has more states |
| Round-robin scheduling | Baseline | ✅ Implemented as fallback | None |
| O(1) scheduler (bitmap + active/expired) | ✅ Full implementation | ✅ Bitmap + active/expired swap, 32 priority levels | None |
| Priority queues (MLFQ) | ✅ 32 priority levels | ✅ 32 priority levels via `SCHED_NUM_PRIOS` | None |
| Unix decay-based priority | ✅ `p_cpu` decay + `nice` | ❌ Not implemented | Enhancement |
| Per-CPU runqueues | ✅ `cpu_runqueue_t` per CPU | ❌ Single global queue | Needed for SMP |
| Sleep/wakeup (wait queues) | ✅ `sleep(chan, lock)` / `wakeup(chan)` | ✅ Process blocking + `nanosleep` syscall | Partial — no generic wait queue abstraction |
| Context switch (assembly) | ✅ Save/restore callee-saved + CR3 | ✅ `context_switch.S` saves/restores regs + CR3 | None |
| `fork()` | ✅ Slab + CoW + enqueue | ✅ `vmm_as_clone_user_cow()` + page fault handler | None |
| `execve()` | ✅ Load ELF, reset stack | ✅ `syscall_execve_impl()` — loads ELF, handles argv/envp, `O_CLOEXEC` | None |
| Spinlock protection | ✅ `sched_lock` | ✅ `sched_lock` present | None |

**Summary:** AdrOS scheduler is now O(1) with bitmap + active/expired arrays and 32 priority levels. Missing decay-based priority and per-CPU runqueues.

---

### 1.5 Signals

| Aspect | Supplementary Suggestion | AdrOS Current State | Gap |
|--------|--------------------------|---------------------|-----|
| Signal bitmask (pending/blocked) | ✅ `uint32_t pending_signals` | ✅ `sig_pending_mask` + `sig_blocked_mask` | None |
| `sigaction` | ✅ Handler array | ✅ `sigactions[PROCESS_MAX_SIG]` | None |
| Signal trampoline | ✅ Build stack frame, redirect EIP | ✅ Full trampoline in `deliver_signals_to_usermode()` | None |
| `sigreturn` | ✅ Restore saved context | ✅ `syscall_sigreturn_impl()` with `SIGFRAME_MAGIC` | None |
| `SA_SIGINFO` | Mentioned | ✅ Supported (siginfo_t + ucontext_t on stack) | None |
| Signal restorer (userspace) | ✅ `sigrestorer.S` | ✅ Kernel injects trampoline code bytes on user stack | AdrOS approach is self-contained |

**Summary:** AdrOS signal implementation is **complete and robust**. This is one of the strongest subsystems — ahead of what the supplementary material suggests.

---

### 1.6 Virtual File System (VFS)

| Aspect | Supplementary Suggestion | AdrOS Current State | Gap |
|--------|--------------------------|---------------------|-----|
| Mount table | ✅ Linked list of mount points | ✅ Up to 8 mounts, longest-prefix matching | None |
| `vfs_lookup` path resolution | ✅ Find mount + delegate to driver | ✅ Full path resolution with mount traversal | None |
| `fs_node_t` with ops | ✅ `vfs_ops_t` function pointers | ✅ `read`/`write`/`open`/`close`/`finddir`/`readdir` | None |
| File descriptor table | ✅ Per-process `fd_table[16]` | ✅ Per-process `files[PROCESS_MAX_FILES]` with refcount | None |
| File cursor (offset) | ✅ `cursor` field | ✅ `offset` in `struct file` | None |
| USTAR InitRD parser | ✅ Full implementation | ❌ Custom binary format (`mkinitrd`) | Different approach, both work |
| LZ4 decompression | ✅ Decompress initrd.tar.lz4 | ❌ Not implemented | Enhancement |
| `pivot_root` | ✅ `sys_pivot_root()` | ❌ Not implemented | Needed for real init flow |
| Multiple FS types | ✅ USTAR + FAT | ✅ tmpfs + devfs + overlayfs + diskfs + persistfs | **AdrOS is ahead** |
| `readdir` generic | Mentioned | ✅ All FS types implement `readdir` callback | None |

**Summary:** AdrOS VFS is **more advanced** than the supplementary material suggests. It has 5 filesystem types, overlayfs, and generic readdir. The supplementary material's USTAR/LZ4 approach is an alternative InitRD strategy.

---

### 1.7 TTY / PTY

| Aspect | Supplementary Suggestion | AdrOS Current State | Gap |
|--------|--------------------------|---------------------|-----|
| Circular buffer for keyboard | ✅ Ring buffer + wait queue | ✅ Ring buffer in `tty.c` with blocking reads | None |
| `tty_push_char` from IRQ | ✅ IRQ1 handler → buffer | ✅ Keyboard IRQ → `tty_input_char()` | None |
| Canonical mode (line editing) | ✅ Buffer until Enter | ✅ Line-buffered with echo + backspace | None |
| PTY master/slave | Not discussed | ✅ Full PTY implementation with `/dev/ptmx` + `/dev/pts/0` | **AdrOS is ahead** |
| Job control (SIGTTIN/SIGTTOU) | Not discussed | ✅ `pty_jobctl_read_check()` / `pty_jobctl_write_check()` | **AdrOS is ahead** |
| `poll()` support | ✅ `tty_poll()` | ✅ `pty_master_can_read()` etc. integrated with `poll` | None |
| Raw mode | Not discussed | ✅ `ICANON` clearable via `TCSETS` | None |
| Signal characters | Not discussed | ✅ Ctrl+C→SIGINT, Ctrl+Z→SIGTSTP, Ctrl+D→EOF, Ctrl+\\→SIGQUIT | **AdrOS is ahead** |
| Window size | Not discussed | ✅ `TIOCGWINSZ`/`TIOCSWINSZ` | **AdrOS is ahead** |

**Summary:** AdrOS TTY/PTY is **significantly ahead** of the supplementary material. Full PTY with job control, raw mode, signal characters, and window size.

---

### 1.8 Spinlocks & Synchronization

| Aspect | Supplementary Suggestion | AdrOS Current State | Gap |
|--------|--------------------------|---------------------|-----|
| `xchg`-based spinlock | ✅ Inline asm `xchgl` | ✅ `__sync_lock_test_and_set` (generates `xchg`) | Equivalent |
| `pause` in spin loop | ✅ `__asm__ volatile("pause")` | ✅ Present in `spin_lock()` | None |
| IRQ save/restore | ✅ `pushcli`/`popcli` with nesting | ✅ `irq_save()`/`irq_restore()` via `pushf`/`popf` | None |
| `spin_lock_irqsave` | ✅ Combined lock + IRQ disable | ✅ `spin_lock_irqsave()` / `spin_unlock_irqrestore()` | None |
| Debug name field | ✅ `char *name` for panic messages | ❌ No name field | Minor |
| CPU ID tracking | ✅ `lock->cpu_id` for deadlock detection | ❌ Not tracked | Enhancement |
| Nesting counter (`ncli`) | ✅ Per-CPU nesting | ❌ Not implemented (flat save/restore) | Enhancement |

**Summary:** AdrOS spinlocks are correct and used throughout the kernel (PMM, heap, slab, scheduler, TTY). SMP-aware features (CPU tracking, nesting) are enhancements.

---

### 1.9 ELF Loader

| Aspect | Supplementary Suggestion | AdrOS Current State | Gap |
|--------|--------------------------|---------------------|-----|
| Parse ELF headers | ✅ `Elf32_Ehdr` + `Elf32_Phdr` | ✅ Full validation + PT_LOAD processing | None |
| Map segments with correct flags | ✅ PF_W → WRITABLE, PF_X → EXECUTABLE | ✅ Maps with `VMM_FLAG_RW`, then `vmm_protect_range()` for .text | None |
| W^X enforcement | ✅ Policy in `vmm_map` | ✅ `.text` marked read-only after copy | Achieved differently |
| Reject kernel-range vaddrs | Not discussed | ✅ Rejects `p_vaddr >= 0xC0000000` | **AdrOS is ahead** |
| User stack allocation | ✅ Mentioned | ✅ Maps user stack at `0x00800000` | None |

**Summary:** AdrOS ELF loader is **complete and secure** with proper validation and W^X enforcement.

---

### 1.10 User-Space / libc

| Aspect | Supplementary Suggestion | AdrOS Current State | Gap |
|--------|--------------------------|---------------------|-----|
| `crt0.S` (entry point) | ✅ `_start` → `main` → `exit` | ✅ `user/crt0.S` with argc/argv setup | None |
| Syscall stub (int 0x80) | ✅ `_syscall_invoke` via registers | ✅ `_syscall` in `user/syscall.S` | None |
| libc wrappers | ✅ `syscalls.c` with errno | ✅ ulibc: `printf`, `malloc`/`free`/`calloc`/`realloc`, `string.h`, `errno.h` | None |
| `init.c` (early userspace) | ✅ mount + pivot_root + execve | ✅ `user/init.c` — comprehensive smoke tests | Different purpose |
| User linker script | ✅ `user.ld` at 0x08048000 | ✅ `user/user.ld` at 0x00400000 | Both valid |
| `SYSENTER` fast path | ✅ vDSO + MSR setup | ✅ `sysenter_init.c` — MSR setup + handler | None |

**Summary:** AdrOS has a working userspace with ulibc, SYSENTER fast path, and a comprehensive test binary. Missing a shell and core utilities.

---

### 1.11 Drivers

| Driver | Supplementary Suggestion | AdrOS Current State |
|--------|--------------------------|---------------------|
| PCI enumeration | ✅ Full scan (bus/dev/func) | ✅ Full scan with BAR + IRQ (`src/hal/x86/pci.c`) |
| ATA DMA (Bus Master IDE) | Not discussed | ✅ Bounce buffer, PRDT, IRQ-coordinated (`src/hal/x86/ata_dma.c`) |
| LAPIC + IOAPIC | Not discussed | ✅ Replaces legacy PIC; IRQ routing |
| SMP (multi-CPU boot) | Not discussed | ✅ 4 CPUs via INIT-SIPI-SIPI, per-CPU data via GS |
| ACPI (MADT parsing) | Not discussed | ✅ CPU topology + IOAPIC discovery |
| VBE/Framebuffer | ✅ Map LFB + MTRR write-combining | ✅ Maps LFB, pixel drawing, font rendering (no MTRR WC) |
| Intel E1000 NIC | ✅ RX/TX descriptor rings + DMA | ❌ Not implemented |
| Intel HDA Audio | ✅ DMA ring buffers | ❌ Not implemented |
| lwIP TCP/IP stack | ✅ `sys_arch.c` bridge | ❌ Not implemented |

---

### 1.12 Advanced Features

| Feature | Supplementary Suggestion | AdrOS Current State |
|---------|--------------------------|---------------------|
| Copy-on-Write (CoW) fork | ✅ Full implementation with ref-counting | ✅ `vmm_as_clone_user_cow()` + `vmm_handle_cow_fault()` |
| Slab allocator | ✅ `slab_cache_t` with free-list-in-place | ✅ `slab_cache_init`/`slab_alloc`/`slab_free` with spinlock |
| Shared memory (shmem/mmap) | ✅ `sys_shmget` / `sys_shmat` | ✅ `shm_get`/`shm_at`/`shm_dt`/`shm_ctl` in `src/kernel/shm.c` |
| Zero-copy DMA I/O | ✅ Map DMA buffer into user VA | ❌ Not implemented |
| vDSO | ✅ Kernel-mapped page with syscall code | ❌ Not implemented |

---

## Part 2 — POSIX Compatibility Assessment

### Overall Score: **~70% toward a practical Unix-like POSIX system**

This score reflects that AdrOS has a **mature kernel** with most core subsystems
implemented and working. The main remaining gaps are userland tooling (shell, core
utilities) and some POSIX interfaces (`mmap`, permissions, links).

### What AdrOS Already Has (Strengths)

1. **Process model** — `fork` (CoW), `execve`, `waitpid`, `exit`, `getpid`, `getppid`, `setsid`, `setpgid`, `getpgrp`, `brk` — all working
2. **File I/O** — `open`, `read`, `write`, `close`, `lseek`, `stat`, `fstat`, `dup`, `dup2`, `dup3`, `pipe`, `pipe2`, `fcntl`, `getdents`, `O_CLOEXEC`, `FD_CLOEXEC` — comprehensive
3. **Signals** — `sigaction`, `sigprocmask`, `kill`, `sigreturn` with full trampoline, Ctrl+C/Z/D signal chars — robust
4. **VFS** — 6 filesystem types (tmpfs, devfs, overlayfs, diskfs, persistfs, procfs), mount table, path resolution — excellent
5. **TTY/PTY** — Line discipline, raw mode, job control, signal chars, `TIOCGWINSZ`, PTY — very good
6. **Select/Poll** — Working for pipes, TTY, PTY, `/dev/null`
7. **Memory management** — PMM (spinlock + refcount), VMM (CoW, recursive PD), heap (dynamic 64MB), slab allocator, SMEP, shared memory
8. **Hardware** — PCI, ATA DMA, LAPIC/IOAPIC, SMP (4 CPUs), ACPI, VBE framebuffer, SYSENTER, CPUID
9. **Userland** — ulibc (`printf`, `malloc`, `string.h`, `errno.h`), ELF loader with W^X
10. **Testing** — 47 host unit tests, 19 smoke tests, cppcheck, GDB scripted checks
11. **Security** — SMEP, user_range_ok hardened, sigreturn eflags sanitized, atomic file refcounts

### What's Missing for Practical POSIX (Gaps by Priority)

#### Tier 1 — Blocks basic usability
| Gap | Impact | Effort |
|-----|--------|--------|
| ~~Minimal libc~~ | ✅ ulibc implemented | Done |
| **Shell** (`sh`-compatible) | No interactive use without it | Medium |
| ~~Signal characters~~ | ✅ Ctrl+C/Z/D implemented | Done |
| ~~`brk`/`sbrk`~~ | ✅ Implemented | Done |
| **Core utilities** (`ls`, `cat`, `echo`, `mkdir`, `rm`) | No file management | Medium |
| **`/dev/zero`** | Missing common device node | Small |
| **Multiple PTY pairs** | Only 1 pair limits multi-process use | Small |

#### Tier 2 — Required for POSIX compliance
| Gap | Impact | Effort |
|-----|--------|--------|
| **`mmap`/`munmap`** | No memory-mapped files | Medium-Large |
| ~~`O_CLOEXEC`~~ | ✅ Implemented | Done |
| **Permissions** (`uid`/`gid`/mode/`chmod`/`chown`) | No multi-user security | Medium |
| **Hard/symbolic links** | Incomplete filesystem semantics | Medium |
| **`/proc` per-process** | Only `/proc/meminfo`, no `/proc/[pid]` | Medium |
| ~~`nanosleep`/`clock_gettime`~~ | ✅ Implemented | Done |
| ~~Raw TTY mode~~ | ✅ Implemented | Done |
| **VMIN/VTIME** | Non-canonical timing not supported | Small |

#### Tier 3 — Full Unix experience
| Gap | Impact | Effort |
|-----|--------|--------|
| ~~CoW fork~~ | ✅ Implemented | Done |
| **PAE + NX bit** | No hardware W^X enforcement | Large |
| ~~Slab allocator~~ | ✅ Implemented | Done |
| **Networking** (socket API + TCP/IP) | No network connectivity | Very Large |
| **Threads** (`clone`/`pthread`) | No multi-threaded programs | Large |
| **Dynamic linking** (`ld.so`) | Can't use shared libraries | Very Large |
| ~~VBE framebuffer~~ | ✅ Implemented | Done |
| ~~PCI + device drivers~~ | ✅ PCI + ATA DMA implemented | Done |

---

## Part 3 — Architectural Comparison Summary

| Dimension | Supplementary Material | AdrOS Current | Verdict |
|-----------|----------------------|---------------|----------|
| **Boot flow** | GRUB → Stub (LZ4) → Kernel → USTAR InitRD | GRUB → Kernel → Custom InitRD → OverlayFS | Both valid; AdrOS is simpler |
| **Memory architecture** | PMM + Slab + CoW + Zero-Copy DMA | PMM (spinlock+refcount) + Slab + CoW + Heap (64MB dynamic) + SMEP + Shmem | **Comparable** (AdrOS missing zero-copy DMA) |
| **Scheduler** | O(1) with bitmap + active/expired arrays | O(1) with bitmap + active/expired, 32 priority levels | **Comparable** (AdrOS missing decay + per-CPU) |
| **VFS** | USTAR + FAT (planned) | tmpfs + devfs + overlayfs + diskfs + persistfs + procfs | **AdrOS is more advanced** |
| **Syscall interface** | int 0x80 + SYSENTER + vDSO | int 0x80 + SYSENTER | **Comparable** (AdrOS missing vDSO) |
| **Signal handling** | Basic trampoline concept | Full SA_SIGINFO + sigreturn + sigframe + signal chars | **AdrOS is more advanced** |
| **TTY/PTY** | Basic circular buffer | Full PTY + raw mode + job control + signal chars + TIOCGWINSZ | **AdrOS is more advanced** |
| **Synchronization** | SMP-aware spinlocks with CPU tracking | Spinlocks with IRQ save, used throughout (PMM, heap, slab, sched) | **Comparable** (AdrOS missing CPU tracking) |
| **Userland** | libc stubs + init + shell concept | ulibc (printf, malloc, string.h) + init + echo | **Comparable** (AdrOS missing shell) |
| **Drivers** | PCI + E1000 + VBE + HDA (conceptual) | PCI + ATA DMA + VBE + LAPIC/IOAPIC + SMP + ACPI | **AdrOS is more advanced** |

---

## Part 4 — Recommendations

### Completed (since initial analysis)

1. ~~Add signal characters to TTY~~ ✅ Ctrl+C/Z/D/\\ implemented
2. ~~Implement `brk`/`sbrk` syscall~~ ✅ `syscall_brk_impl()`
3. ~~Build minimal libc~~ ✅ ulibc with printf, malloc, string.h, errno.h
4. ~~PMM ref-counting~~ ✅ `pmm_incref`/`pmm_decref`/`pmm_get_refcount`
5. ~~CoW fork~~ ✅ `vmm_as_clone_user_cow()` + `vmm_handle_cow_fault()`
6. ~~O(1) scheduler~~ ✅ Bitmap + active/expired arrays, 32 priority levels
7. ~~Slab allocator~~ ✅ `slab_cache_init`/`slab_alloc`/`slab_free`
8. ~~PCI enumeration~~ ✅ Full bus/slot/func scan
9. ~~CPUID~~ ✅ Leaf 0/1/7/extended, SMEP enabled

### Immediate Actions (next priorities)

1. **Build a shell** — All required syscalls are implemented. This is the single biggest usability unlock.

2. **Core utilities** — `ls`, `cat`, `echo`, `mkdir`, `rm`. All required VFS operations exist.

3. **`/dev/zero`** + **`/dev/random`** — Simple device nodes, small effort.

4. **Multiple PTY pairs** — Currently only 1 pair. Needed for multi-process terminal use.

### Medium-Term

5. **`mmap`/`munmap`** — Requires `vmm_find_free_area()`. Critical for POSIX.

6. **Permissions model** — `uid`/`gid`/mode bits, `chmod`, `chown`, `access`, `umask`.

7. **`/proc` per-process** — `/proc/[pid]/status`, `/proc/[pid]/maps`.

8. **Hard/symbolic links** — `link`, `symlink`, `readlink`.

### Long-Term

9. **PAE + NX** — Hardware W^X enforcement.

10. **Networking** — E1000 DMA ring → lwIP bridge → socket API.

11. **Threads** — `clone`/`pthread`.

12. **Dynamic linking** — `ld.so`.

---

## Conclusion

AdrOS is a **mature hobby OS** that has implemented most of the hard parts of a Unix-like
system: CoW fork, O(1) scheduler, slab allocator, SMP boot, PCI/DMA drivers, full signal
handling, a multi-filesystem VFS, PTY with job control, and a secure ELF loader. It is
approximately **70% of the way** to a practical POSIX-compatible system.

The supplementary material's architectural blueprints have been largely **realized**:
CoW memory, O(1) scheduling, slab allocator, PCI enumeration, and CPUID detection are
all implemented. AdrOS is **ahead** of the supplementary material in several areas
(VFS diversity, signal handling, TTY/PTY, driver support, SMP).

The most impactful next steps are **userland tooling**: a shell and core utilities.
With ulibc already providing `printf`/`malloc`/`string.h`, and all required syscalls
implemented, building a functional shell is now straightforward. This would transform
AdrOS from a kernel with smoke tests into an **interactive Unix system**.
