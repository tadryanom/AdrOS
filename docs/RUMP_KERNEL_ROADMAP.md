# Rump Kernel Integration Roadmap

## Overview

[Rump Kernels](https://github.com/rumpkernel/wiki) allow AdrOS to reuse production-quality
NetBSD drivers (USB, audio, ZFS, TCP/IP, etc.) without writing them from scratch. The
integration requires implementing a **hypercall layer** (`librumpuser_adros`) that maps
NetBSD kernel abstractions to AdrOS primitives.

## Prerequisites Status

| Requirement | Status | AdrOS Implementation |
|---|---|---|
| Dynamic memory (malloc/free) | ✅ Ready | `kmalloc`/`kfree` (buddy allocator, 16-byte aligned) |
| Kernel threads | ✅ Ready | `process_create_kernel`, `PROCESS_FLAG_THREAD` |
| Mutexes | ✅ Ready | `kmutex_t` in `sync.c` |
| Semaphores | ✅ Ready | `ksem_t` with timeout in `sync.c` |
| Condition variables | ✅ Ready | `kcond_t` (wait/signal/broadcast) in `sync.c` |
| FPU/SSE context switch | ✅ Ready | FXSAVE/FXRSTOR per process |
| Nanosecond timekeeping | ✅ Ready | TSC-calibrated `clock_gettime_ns()` |
| Shared IRQ handling | ✅ Ready | IRQ chaining (32-node pool) in `idt.c` |
| Block I/O | ✅ Ready | ATA multi-drive, VFS callbacks |
| PCI enumeration | ✅ Ready | PCI scanner + HAL driver registry |

## Rumpuser Hypercall API → AdrOS Mapping

The `rumpuser(3)` interface requires ~35 functions grouped into 8 categories:

### Phase 1: Core (Memory + Console + Init)
```
rumpuser_init()            → validate version, store upcalls
rumpuser_malloc()          → kmalloc() with alignment
rumpuser_free()            → kfree()
rumpuser_putchar()         → kprintf("%c", ch)
rumpuser_dprintf()         → kprintf()
rumpuser_exit()            → kprintf + halt/panic
rumpuser_getparam()        → return NCPU, hostname from cmdline
rumpuser_getrandom()       → /dev/urandom or RDRAND
```

### Phase 2: Threads + Synchronization
```
rumpuser_thread_create()   → process_create_kernel() wrapper
rumpuser_thread_exit()     → process_exit_notify() + schedule()
rumpuser_thread_join()     → waitpid() equivalent
rumpuser_curlwpop()        → per-thread TLS via GS segment
rumpuser_curlwp()          → read from TLS
rumpuser_mutex_init()      → kmutex_init() or spinlock_init()
rumpuser_mutex_enter()     → kmutex_lock() (unschedule rump ctx first)
rumpuser_mutex_exit()      → kmutex_unlock()
rumpuser_mutex_owner()     → track owner in wrapper struct
rumpuser_rw_init()         → custom reader-writer lock
rumpuser_rw_enter()        → kcond_t based rwlock
rumpuser_cv_init()         → kcond_init()
rumpuser_cv_wait()         → kcond_wait()
rumpuser_cv_timedwait()    → kcond_wait() with timeout
rumpuser_cv_signal()       → kcond_signal()
rumpuser_cv_broadcast()    → kcond_broadcast()
```

### Phase 3: Clocks + Signals
```
rumpuser_clock_gettime()   → clock_gettime_ns() (MONO) or rtc (WALL)
rumpuser_clock_sleep()     → process_sleep() with ns precision
rumpuser_kill()            → process_kill() signal delivery
rumpuser_seterrno()        → per-thread errno via TLS
```

### Phase 4: File/Block I/O
```
rumpuser_open()            → vfs_open() or raw device access
rumpuser_close()           → vfs_close()
rumpuser_getfileinfo()     → vfs_stat()
rumpuser_bio()             → async ATA I/O + callback
rumpuser_iovread()         → vfs_read() scatter-gather
rumpuser_iovwrite()        → vfs_write() scatter-gather
rumpuser_syncfd()          → flush + barrier
```

## Implementation Plan

### Stage 1: Filesystem (ext2 via Rump) — weeks 1-2
**Goal**: Mount an ext2 image using NetBSD's ext2fs driver via Rump instead of AdrOS native.
- Implement Phase 1 + 2 + 3 hypercalls
- Build `librumpuser_adros.a` statically linked into kernel
- Cross-compile `librump.a` + `librumpvfs.a` + `librumpfs_ext2fs.a`
- Test: mount ext2 disk image, read files, compare with native driver

**Why first**: Exercises memory, threads, and synchronization without hardware IRQs.

### Stage 2: Network (E1000 via Rump) — weeks 3-4
**Goal**: Replace lwIP+custom E1000 driver with NetBSD's full TCP/IP stack.
- Implement Phase 4 (block I/O) hypercalls
- Cross-compile `librumpnet.a` + `librumpdev_pci.a` + `librumpdev_wm.a`
- Wire PCI device passthrough via rumpuser_open/bio
- Test: ping, TCP connect through Rump network stack

**Why second**: Adds IRQ sharing and async I/O, leveraging existing IOAPIC routing.

### Stage 3: USB (xHCI/EHCI via Rump) — weeks 5-8
**Goal**: USB mass storage support.
- Cross-compile USB host controller + mass storage components
- Map PCI MMIO regions for USB controller
- DMA buffer management via rumpuser_malloc with alignment
- Test: enumerate USB devices, mount USB mass storage

### Stage 4: Audio (HDA via Rump) — optional
**Goal**: Intel HDA audio playback.
- Leverages same PCI/IRQ infrastructure as Stage 2-3

## Build Integration

```
src/rump/
  rumpuser_adros.c      # All hypercall implementations
  rumpuser_adros.h      # Internal helpers
  rump_integration.c    # Boot-time init, component loading
third_party/rump/
  include/              # NetBSD rump headers
  lib/                  # Pre-built librump*.a archives
```

## Missing Kernel Primitives (to implement as needed)

| Primitive | Needed For | Complexity |
|---|---|---|
| Reader-writer lock (`krwlock_t`) | `rumpuser_rw_*` | Small |
| Per-thread TLS storage | `rumpuser_curlwp` | Small (GS segment) |
| Aligned kmalloc | `rumpuser_malloc` with alignment | Small (round up) |
| Async block I/O callback | `rumpuser_bio` | Medium |

## References

- [rumpuser(3) manpage](https://man.netbsd.org/rumpuser.3)
- [buildrump.sh](https://github.com/rumpkernel/buildrump.sh)
- [Rump Kernel wiki](https://github.com/rumpkernel/wiki/wiki)
- Antti Kantee, "The Design and Implementation of the Anykernel and Rump Kernels", 2012
