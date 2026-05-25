# AdrOS Audit Fix Plan

Consolidated from the 2026-05-20 technical audit. Issues ordered by severity
and exploitability. Each entry references the exact code location and proposed
fix.

---

## Round 1 — CRITICAL: Kernel Memory Isolation & W^X

### K01: mmap MAP_FIXED end address not validated

**File**: `src/kernel/syscall.c:3040-3043`

**Bug**: Only `addr` is checked against `kernel_base`. The end address
`addr + aligned_len` can cross into kernel space, allowing a user process to
overwrite kernel page tables.

**Fix**: After the existing `addr >= kernel_base` check, add:
```c
if (addr + aligned_len > hal_mm_kernel_virt_base())
    return (uintptr_t)-EINVAL;
```
Also validate that `addr + aligned_len` does not wrap around (overflow).

---

### K02: mprotect range crosses kernel boundary

**File**: `src/kernel/syscall.c:4176-4222`

**Bug**: The permissive stack check at line 4209 (`addr >= 0x08000000U &&
addr < kern_base`) allows any range in that region, even if `addr + aligned_len`
extends past `kern_base` into kernel space. `vmm_protect_range` would then
modify kernel page flags.

**Fix**:
1. Add end-address validation: `if (addr + aligned_len > kern_base) return -ENOMEM;`
2. Remove the permissive fallback or restrict it to only the actual stack
   region (track stack bottom in `struct process`).

---

### K03: shm_at maps without address validation

**File**: `src/kernel/shm.c:150-162`

**Bug**: User-supplied `shmaddr` is used directly without checking alignment
or kernel boundary. The auto-assigned address (`0x40000000U + mslot * ...`)
can also overlap existing mappings or exceed user space.

**Fix**:
1. If `shmaddr != 0`, validate alignment (`shmaddr & 0xFFF == 0`) and
   `shmaddr + seg->npages * PAGE_SIZE <= kernel_base`.
2. For auto-assignment, use `vmm_find_free_area` instead of a fixed formula.
3. Check that the range doesn't overlap existing mmap entries.

---

### A01: NX flag lost on COW clone and COW fault

**File**: `src/arch/x86/vmm.c:446-459` (clone), `:505,525` (fault handler)

**Bug**: `vmm_as_clone_user_cow` builds `new_pte` with only
`PRESENT | USER | COW`, discarding the NX bit from the original PTE.
`vmm_handle_cow_fault` maps the new page with `PRESENT | RW | USER`,
also losing NX. This breaks W^X: executable+writable pages in the parent
become writable+executable in the child after COW resolution.

**Fix**:

In `vmm_as_clone_user_cow` (line 446):
```c
uint64_t new_pte = (uint64_t)frame_phys | X86_PTE_PRESENT | X86_PTE_USER;
if (pte & X86_PTE_RW) {
    new_pte |= X86_PTE_COW;
    // Preserve NX from original
    if (pte & X86_PTE_NX) new_pte |= X86_PTE_NX;
    src_pt[ti] = new_pte;
    invlpg(va);
} else {
    new_pte = pte;  // already preserves NX
}
```

And in `vmm_as_map_page_nolock`, pass `VMM_FLAG_NX` when `new_pte & X86_PTE_NX`.

In `vmm_handle_cow_fault` (lines 505 and 525):
```c
// Preserve NX from original PTE
uint64_t nx = pte & X86_PTE_NX;
// rc <= 1 path:
pt[ti] = (uint64_t)old_frame | X86_PTE_PRESENT | X86_PTE_RW | X86_PTE_USER | nx;
// copy path:
pt[ti] = (uint64_t)(uintptr_t)new_frame | X86_PTE_PRESENT | X86_PTE_RW | X86_PTE_USER | nx;
```

---

## Round 2 — HIGH: Permission & Access Control

### A03: read/write ignore fd open mode

**File**: `src/kernel/syscall.c:2740-2806` (read), `:2810-2861` (write)

**Bug**: `syscall_read_impl` does not check if the fd was opened `O_WRONLY`,
and `syscall_write_impl` does not check if the fd was opened `O_RDONLY`.

**Fix**:

In `syscall_read_impl`, after `fd_get`:
```c
if ((f->flags & 3U) == 1U) return -EBADF;  /* O_WRONLY */
```

In `syscall_write_impl`, after `fd_get`:
```c
if ((f->flags & 3U) == 0U) return -EBADF;  /* O_RDONLY */
```

---

### A06: kill syscall lacks permission checks

**File**: `src/kernel/scheduler.c:365-424`

**Bug**: `process_kill` sends signals to any process without checking
caller credentials. Any user can kill any other user's processes (or root's).

**Fix**: Add permission check in `process_kill` before setting
`sig_pending_mask`:
```c
/* Permission check (POSIX): sender must be root, or same uid, or same euid */
if (current_process && current_process->euid != 0) {
    if (current_process->euid != p->uid && current_process->uid != p->uid)
        return -EPERM;
}
/* SIGCONT is special: can always be sent if same session (skip for now) */
```
Same check needed in `process_kill_pgrp`.

---

### SYSENTER: user stack pointer not validated

**File**: `src/arch/x86/sysenter.S:84-85`

**Bug**: `mov 4(%ecx), %edx` and `mov 8(%ecx), %ecx` read from the user ESP
without validating that ECX points to user space. A malicious ECX pointing
into kernel memory leaks kernel data into syscall arguments.

**Fix**: Add validation in the assembly entry or in `syscall_handler`:
- In `sysenter_entry`, after pushing the iret frame, check that ECX is
  below `KERNEL_VIRT_BASE` before dereferencing. If not, set ECX/EDX to 0
  and set EAX to -EFAULT.
- Alternatively, validate in C before the `mov 4(%ecx)` / `mov 8(%ecx)`
  by using `copy_from_user` to read arg2/arg3 from the saved user ESP
  in the registers struct.

---

### AIO: passes user buffer directly to VFS

**File**: `src/kernel/syscall.c:1656-1666`

**Bug**: `syscall_aio_rw_impl` passes `cb.aio_buf` (a user-space pointer)
directly to `f->node->f_ops->read/write`. The VFS function then reads/writes
from/to user memory without `copy_from_user`/`copy_to_user`, bypassing SMAP.

**Fix**: Allocate a kernel bounce buffer, `copy_from_user` into it (for
writes), call VFS with the kernel buffer, then `copy_to_user` (for reads).

---

### Socket send/recv: passes user buffer directly to lwIP

**File**: `src/kernel/syscall.c:4695,4711` → `src/kernel/socket.c:377-432`

**Bug**: `ksocket_send` passes the user buffer directly to `tcp_write` and
`memcpy(p->payload, buf, len)`. `ksocket_recv` writes directly to the user
buffer via `rxbuf_read`. With SMAP enabled, the `user_range_ok` check is
done but the actual access bypasses `copy_from_user`/`copy_to_user`.

**Fix**:
- For send: `copy_from_user` into a kernel buffer, then pass to lwIP.
- For recv: `rxbuf_read` into a kernel buffer, then `copy_to_user`.

---

### vDSO tick_hz mismatch

**File**: `src/kernel/vdso.c:35` vs `include/timer.h:15`

**Bug**: `vdso_kptr->tick_hz = 50` but `TIMER_HZ = 100`. User-space time
calculations using vDSO will be off by 2x.

**Fix**: Replace hardcoded 50 with `TIMER_HZ`:
```c
vdso_kptr->tick_hz = TIMER_HZ;
```
Add `#include "timer.h"` to `vdso.c`.

---

## Round 3 — MEDIUM: POSIX Compliance & Robustness

### truncate/ftruncate: no write permission check

**File**: `src/kernel/syscall.c:4299-4323`

**Bug**: Neither syscall checks that the fd was opened for writing, or that
the caller has write permission on the file.

**Fix**:
- `ftruncate`: check `(f->flags & 3U) == 0U` (O_RDONLY) → return -EBADF.
- `truncate`: `vfs_check_permission(node, 2)` (write) → return -EACCES.

---

### O_EXCL not enforced

**File**: `src/kernel/syscall.c:2361-2365`

**Bug**: When `O_CREAT | O_EXCL` is specified and the file already exists,
the open should fail with `-EEXIST`. Currently it succeeds (falls through
to the O_TRUNC check or just opens normally).

**Fix**: After `vfs_lookup` finds the node:
```c
if (node && (flags & 0x40U) && (flags & 0x80U))  /* O_CREAT | O_EXCL */
    return -EEXIST;
```

---

### O_NOFOLLOW not enforced

**File**: `src/kernel/syscall.c:2341-2404`

**Bug**: `O_NOFOLLOW` (0x20000) should cause open to fail with `-ELOOP`
if the path refers to a symlink. AdrOS doesn't have symlinks yet, so this
is a no-op for now but the flag should be accepted silently.

**Status**: Deferred (no symlink support yet).

---

### O_DIRECTORY not enforced

**File**: `src/kernel/syscall.c:2341-2404`

**Bug**: `O_DIRECTORY` (0x10000) should fail with `-ENOTDIR` if the path
is not a directory.

**Fix**: After `vfs_lookup`:
```c
if ((flags & 0x10000U) && node && !(node->flags & FS_DIRECTORY))
    return -ENOTDIR;
```

---

### posix_spawn wrapper broken

**File**: `user/ulibc/src/spawn.c:22`

**Bug**: Uses `_syscall2(SYS_POSIX_SPAWN, path, argv)` but the kernel
handler expects 4 arguments (pid_out, path, argv, envp). The missing
arguments cause the child to exec with garbage envp, and the parent
doesn't get the child PID.

**Fix**: Change to `_syscall4`:
```c
int ret = _syscall4(SYS_POSIX_SPAWN, (int)pid, (int)path, (int)argv, (int)envp);
```

---

### SYSCALL_MKDIR ignores mode argument

**File**: `src/kernel/syscall.c:3693-3696`

**Bug**: `syscall_mkdir_impl` doesn't receive or pass the mode argument.

**Fix**: Read mode from `sc_arg1(regs)` and pass it through to `vfs_mkdir`.
Update `vfs_mkdir` signature to accept mode_t.

---

### CLONE_VM address space refcount missing

**File**: `src/kernel/scheduler.c:772-774,338-344`

**Bug**: When `CLONE_VM` is set, the child shares the parent's `addr_space`
pointer. When any thread exits, `process_reap` checks `PROCESS_FLAG_THREAD`
and skips `vmm_as_destroy`. But if the thread group leader exits before
other threads, its `process_reap` destroys the address space while other
threads still reference it.

**Fix**: Add a `uint32_t as_refcount` to `struct process` (or a separate
refcount table). Increment on CLONE_VM, decrement on reap. Only destroy
when refcount reaches 0.

---

## Round 4 — LOW: Hardening & Cleanup

### Saved set-user-ID not implemented

**Bug**: `setuid`/`seteuid` don't maintain the POSIX saved set-user-ID.
After `seteuid(uid)`, there's no way back without being root.

**Fix**: Add `suid`/`sgid` fields to `struct process`. On `setuid`/`seteuid`,
save the old euid to suid. Allow `seteuid(suid)` without root.

---

### No kernel reboot() syscall

**Bug**: `init` handles SIGUSR1/SIGUSR2 by killing all processes and
calling `_exit(0)`, but the kernel never actually reboots or powers off.

**Fix**: Add `SYSCALL_REBOOT` that calls ACPI shutdown or keyboard
controller reset.

---

### socket_syscall_dispatch misnamed

**File**: `src/kernel/syscall.c`

**Bug**: `socket_syscall_dispatch` handles MQ, SEM, DLOPEN, EPOLL, INOTIFY,
AIO, MOUNT, PIVOT_ROOT, etc. — not just sockets.

**Fix**: Rename to `extended_syscall_dispatch`.

---

## Test Plan

After each round, run:
```bash
make iso && rm -f serial.log && make test          # QEMU smoke
make test-battery                                   # Extended
make test-host                                      # Host unit tests
cppcheck --enable=all --suppress=unusedFunction \
  --suppress=missingIncludeSystem \
  -I include -I user/ulibc/include src/ user/       # Static analysis
```

### New regression tests to add:

| Test | Validates |
|------|-----------|
| mmap MAP_FIXED crossing kernel base | K01 |
| mprotect range crossing kernel base | K02 |
| shmat with kernel-space address | K03 |
| fork preserves NX on code pages | A01 |
| read on O_WRONLY fd returns EBADF | A03 |
| write on O_RDONLY fd returns EBADF | A03 |
| kill from non-root to root returns EPERM | A06 |
| O_CREAT+O_EXCL on existing file returns EEXIST | O_EXCL |
| ftruncate on O_RDONLY fd returns EBADF | truncate |
| posix_spawn returns child PID and execs | posix_spawn |
