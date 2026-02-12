# AdrOS Deep Code Audit Report

## 1. Layer Violations (Arch-Dependent Code in Arch-Independent Files)

### 1.1 CRITICAL — `src/kernel/syscall.c` uses x86 register names directly

```c
// Line 18-20: #ifdef __i386__ with extern x86_sysenter_init
// Line 183: child_regs.eax = 0;
// Line 740-742: regs->eip, regs->useresp, regs->eax
// Line 1603+: regs->eax, regs->ebx, regs->ecx, regs->edx, regs->esi
// Line 1987-1991: syscall_init() with #if defined(__i386__)
```

**Impact**: `syscall.c` is in `src/kernel/` (arch-independent) but references x86 register
names (`eax`, `ebx`, `ecx`, `edx`, `esi`, `eip`, `useresp`) throughout the entire
`syscall_handler()` function. This makes the file completely non-portable.

**Fix**: Define a `struct syscall_args` in a HAL header with generic field names
(`arg0`–`arg5`, `result`, `ip`, `sp`), and have each arch's interrupt stub populate it.

### 1.2 MODERATE — `src/mm/heap.c` hardcodes x86 virtual address

```c
// Line 12: #define KHEAP_START 0xD0000000
```

**Impact**: The heap start address `0xD0000000` is specific to the x86 higher-half kernel
layout. ARM/RISC-V/MIPS would use different virtual address ranges.

**Fix**: Move `KHEAP_START` to a per-arch `include/arch/<ARCH>/mm_layout.h` or define it
via the HAL (`hal_mm_heap_start()`).

### 1.3 MODERATE — `include/interrupts.h` conditionally includes x86 IDT header

```c
// Line 6-7: #if defined(__i386__) ... #include "arch/x86/idt.h"
```

**Impact**: The generic `interrupts.h` pulls in the full x86 `struct registers` (with
`eax`, `ebx`, etc.) into every file that includes `process.h`. This is the root cause
of 1.1 — the x86 register layout leaks into all kernel code.

**Fix**: Define a generic `struct trap_frame` in `interrupts.h` with arch-neutral names.
Each arch provides its own mapping.

### 1.4 LOW — `include/io.h` includes `arch/x86/io.h` unconditionally

```c
// The generic io.h provides MMIO helpers but also pulls in x86 port I/O
```

**Impact**: Non-x86 architectures don't have port I/O. The `#include "arch/x86/io.h"` is
guarded by `#if defined(__i386__)` so it compiles, but the design leaks.

### 1.5 LOW — `src/kernel/syscall.c` line 994: `strcpy(s, tmp)` in `path_normalize_inplace`

The function writes to a local `char tmp[128]` then copies back with `strcpy(s, tmp)`.
If `s` is shorter than 128 bytes, this is a buffer overflow. Currently `s` is always
128 bytes (from `process.cwd` or local buffers), but the function signature doesn't
enforce this.

---

## 2. Logic Errors and Race Conditions

### 2.1 CRITICAL — PMM `pmm_alloc_page` / `pmm_free_page` have no locking

```c
// src/mm/pmm.c: No spinlock protects the bitmap or frame_refcount arrays
```

**Impact**: On SMP (4 CPUs active), concurrent `pmm_alloc_page()` calls can return the
same physical frame to two callers, causing memory corruption. `pmm_free_page()` can
corrupt `used_memory` counter. `pmm_incref`/`pmm_decref` use atomics for refcount but
`bitmap_set`/`bitmap_unset` are NOT atomic — byte-level read-modify-write races.

**Fix**: Add a `spinlock_t pmm_lock` and wrap `pmm_alloc_page`, `pmm_free_page`,
`pmm_mark_region` with `spin_lock_irqsave`/`spin_unlock_irqrestore`.

### 2.2 CRITICAL — `file->refcount` manipulation is not atomic

```c
// src/kernel/syscall.c line 197: f->refcount++;  (no lock)
// src/kernel/syscall.c line 762: f->refcount++;  (no lock)
// src/kernel/scheduler.c line 148: f->refcount--; (under sched_lock, but other
//   increments happen outside sched_lock)
```

**Impact**: `f->refcount++` in `syscall_fork_impl` and `syscall_dup2_impl` runs without
any lock. If a timer interrupt fires and `schedule()` runs `process_close_all_files_locked`
concurrently, the refcount can go negative or skip zero, leaking file descriptors or
causing use-after-free.

**Fix**: Use `__sync_fetch_and_add` / `__sync_sub_and_fetch` for refcount, or protect
all refcount manipulation with a dedicated `files_lock`.

### 2.3 HIGH — Slab allocator uses `hal_mm_phys_to_virt` without VMM mapping

```c
// src/mm/slab.c line 39: uint8_t* vbase = (uint8_t*)hal_mm_phys_to_virt(...)
```

**Impact**: Same bug as the DMA heap collision. If `pmm_alloc_page()` returns a physical
address above 16MB, `hal_mm_phys_to_virt` adds `0xC0000000` which can land in the heap
VA range (`0xD0000000`+) or other mapped regions. The slab then corrupts heap memory.

**Fix**: Either allocate slab pages from the heap (`kmalloc(PAGE_SIZE)`), or use
`vmm_map_page` to map at a dedicated VA range (like the DMA fix).

### 2.4 HIGH — `process_waitpid` loop can miss NULL in circular list

```c
// src/kernel/scheduler.c line 267: } while (it != start);
```

**Impact**: The inner loop `do { ... } while (it != start)` doesn't check `it != NULL`
before dereferencing. If the circular list is broken (e.g., a process was reaped
concurrently), this causes a NULL pointer dereference.

### 2.5 MODERATE — `schedule()` unlocks spinlock before `context_switch`

```c
// src/kernel/scheduler.c line 621: spin_unlock_irqrestore(&sched_lock, irq_flags);
// line 623: context_switch(&prev->sp, current_process->sp);
```

**Impact**: Between unlock and context_switch, another CPU can modify `current_process`
or the process being switched to. On SMP this is a race window. Currently only BSP runs
the scheduler, so this is latent.

### 2.6 MODERATE — `itoa` has no buffer size parameter

```c
// src/kernel/utils.c line 64: void itoa(int num, char* str, int base)
```

**Impact**: `itoa` writes to `str` without knowing its size. For `INT_MIN` in base 10,
it writes 12 characters (`-2147483648\0`). Many call sites use `char tmp[12]` which is
exactly enough, but `char tmp[11]` would overflow. No safety margin.

### 2.7 MODERATE — `itoa` undefined behavior for `INT_MIN`

```c
// src/kernel/utils.c line 77: num = -num;
```

**Impact**: When `num == INT_MIN` (-2147483648), `-num` is undefined behavior in C
(signed integer overflow). On x86 with two's complement it happens to work, but it's
technically UB.

### 2.8 LOW — `pmm_alloc_page_low` in VMM wastes pages

```c
// src/arch/x86/vmm.c line 59-71: pmm_alloc_page_low()
```

**Impact**: Allocates pages and immediately frees them if above 16MB. Under memory
pressure this can loop 1024 times, freeing pages that other CPUs might need. Not a
correctness bug but a performance/reliability issue.

---

## 3. Security Vulnerabilities

### 3.1 CRITICAL — Weak `user_range_ok` allows kernel memory access

```c
// src/kernel/uaccess.c line 17-24:
int user_range_ok(const void* user_ptr, size_t len) {
    uintptr_t uaddr = (uintptr_t)user_ptr;
    if (len == 0) return 1;
    if (uaddr == 0) return 0;
    uintptr_t end = uaddr + len - 1;
    if (end < uaddr) return 0;  // overflow check
    return 1;  // <-- ALWAYS returns 1 for non-NULL, non-overflow!
}
```

**Impact**: This function does NOT check that the address is in user space (below
`0xC0000000` on x86). A malicious userspace program can pass kernel addresses
(e.g., `0xC0100000`) to `read()`/`write()` syscalls and read/write arbitrary kernel
memory. **This is a privilege escalation vulnerability.**

The x86-specific override in `src/arch/x86/uaccess.c` may fix this, but the weak
default is dangerous if the override is not linked.

**Fix**: The weak default must reject addresses >= `KERNEL_VIRT_BASE`. Better: always
link the arch-specific version.

### 3.2 CRITICAL — `sigreturn` allows arbitrary register restoration

```c
// src/kernel/syscall.c line 1380-1398: syscall_sigreturn_impl
```

**Impact**: The sigreturn syscall restores ALL registers from a user-provided
`sigframe`. While it checks `cs & 3 == 3` and `ss & 3 == 3`, it does NOT validate
`eflags`. A user can set `IOPL=3` in the saved eflags, gaining direct port I/O access
from ring 3. This allows arbitrary hardware access.

**Fix**: Mask eflags: `f.saved.eflags = (f.saved.eflags & ~0x3000) | 0x200;` (clear
IOPL, ensure IF).

### 3.3 HIGH — `execve` writes to user stack via kernel pointer

```c
// src/kernel/syscall.c line 697: memcpy((void*)sp, kenvp[i], len);
// line 705: memcpy((void*)sp, kargv[i], len);
// line 714: memcpy((void*)sp, envp_ptrs_va, ...);
// line 724: *(uint32_t*)sp = (uint32_t)argc;
```

**Impact**: After `vmm_as_activate(new_as)`, the code writes directly to user-space
addresses via `memcpy((void*)sp, ...)`. This bypasses `copy_to_user` and its
validation. If the new address space mapping is incorrect, this could write to
kernel memory.

### 3.4 HIGH — No SMEP/SMAP enforcement

**Impact**: The kernel doesn't enable SMEP (Supervisor Mode Execution Prevention) or
SMAP (Supervisor Mode Access Prevention) even if the CPU supports them. Without SMEP,
a kernel exploit can jump to user-mapped code. Without SMAP, accidental kernel reads
from user pointers succeed silently.

### 3.5 MODERATE — `fd_get` doesn't validate fd bounds everywhere

Many syscall implementations call `fd_get(fd)` but some paths (like the early
`fd == 1 || fd == 2` check in `syscall_write_impl`) access `current_process->files[fd]`
directly without bounds checking `fd < PROCESS_MAX_FILES`.

### 3.6 MODERATE — `path_normalize_inplace` doesn't prevent path traversal to kernel FS

The path normalization resolves `..` but doesn't prevent accessing sensitive kernel
mount points. A user can `open("/proc/self/status")` which is fine, but there's no
permission model — any process can read any file.

---

## 4. Memory Management Issues

### 4.1 HIGH — Heap never grows

```c
// src/mm/heap.c: KHEAP_INITIAL_SIZE = 10MB, no growth mechanism
```

**Impact**: The kernel heap is fixed at 10MB. Once exhausted, all `kmalloc` calls fail.
There's no mechanism to map additional pages and extend the heap. For a kernel with
many processes, 10MB can be tight.

### 4.2 MODERATE — `kfree` doesn't zero freed memory

**Impact**: Freed heap blocks retain their old contents. If a new allocation reuses the
block, it may contain sensitive data from a previous allocation (information leak
between processes via kernel allocations).

### 4.3 MODERATE — No stack guard pages — **FIXED**

User stacks now have a 4KB unmapped guard page below the 32KB stack region.
Stack overflow triggers a page fault → SIGSEGV instead of silent corruption.
Kernel stacks (4KB) still lack guard pages — enhancement for the future.

---

## 5. Miscellaneous Issues

### 5.1 `proc_meminfo_read` reads `ready_queue_head` without lock

```c
// src/kernel/procfs.c line 108-114: iterates process list without sched_lock
```

### 5.2 `process_kill` self-kill path calls `schedule()` without lock

```c
// src/kernel/scheduler.c line 166-169: process_exit_notify + schedule without lock
```

### 5.3 `tmpfs_node_alloc` uses unbounded `strcpy`

```c
// src/kernel/tmpfs.c line 29: strcpy(n->vfs.name, name);
```

If `name` exceeds 128 bytes (the size of `fs_node.name`), this overflows.

---

## 6. Summary Table

| # | Severity | Category | Location | Description | Status |
|---|----------|----------|----------|-------------|--------|
| 3.1 | CRITICAL | Security | uaccess.c | user_range_ok allows kernel addr | **FIXED** |
| 3.2 | CRITICAL | Security | syscall.c | sigreturn allows IOPL escalation | **FIXED** |
| 2.1 | CRITICAL | Race | pmm.c | No locking on PMM bitmap | **FIXED** |
| 2.2 | CRITICAL | Race | syscall.c | file refcount not atomic | **FIXED** |
| 1.1 | CRITICAL | Layer | syscall.c | x86 registers in generic code | Open |
| 2.3 | HIGH | Memory | slab.c | phys_to_virt can hit heap VA | **FIXED** |
| 3.3 | HIGH | Security | syscall.c | execve bypasses copy_to_user | **FIXED** |
| 3.4 | HIGH | Security | - | No SMEP/SMAP | **SMEP FIXED** |
| 4.1 | HIGH | Memory | heap.c | Heap never grows | **FIXED** |
| 2.4 | HIGH | Logic | scheduler.c | waitpid NULL deref risk | **FIXED** |
| 1.2 | MODERATE | Layer | heap.c | Hardcoded heap VA | Open |
| 1.3 | MODERATE | Layer | interrupts.h | x86 registers leak | Open |
| 2.5 | MODERATE | Race | scheduler.c | Unlock before context_switch | Open |
| 2.6 | MODERATE | Logic | utils.c | itoa no buffer size | Open |
| 2.7 | MODERATE | Logic | utils.c | itoa UB for INT_MIN | Open |
| 3.5 | MODERATE | Security | syscall.c | fd bounds not always checked | Open |
| 4.2 | MODERATE | Memory | heap.c | kfree doesn't zero | Open |
| 4.3 | MODERATE | Memory | scheduler.c | No stack guard pages | **USER FIXED** (kernel stacks still open) |

## 7. Fix Summary

**4 CRITICAL fixed**: user_range_ok kernel addr check, sigreturn eflags sanitization,
PMM spinlock, file refcount atomics.

**5 HIGH fixed**: slab uses kmalloc instead of phys_to_virt, execve sp bounds check,
SMEP enabled via CR4, heap grows dynamically to 64MB, waitpid NULL guard.

**1 MODERATE fixed**: User stack guard pages implemented (unmapped page below 32KB stack).

**Remaining**: 1 CRITICAL (layer violation — arch refactor), 7 MODERATE (open).
SMAP not yet enabled (SMEP is active). Kernel stacks still lack guard pages.
