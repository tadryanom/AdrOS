# AdrOS — Tier 6 Implementation Plan

**Date:** 2026-03-14
**Prerequisite:** Tiers 1-5 complete (commit aa5474a), audit updated (commit f3a652e)
**Current state:** ~99% POSIX, 139 syscalls, 101 smoke tests, all ulibc headers complete

---

## Overview

Tier 6 contains **large architectural features** that go beyond POSIX gap-filling.
Each sub-tier is independent and can be implemented in any order.

---

## 6A. Full SMP Scheduling (Priority: High, Effort: Large — 1-2 weeks) — ✅ DONE

**Current state:** 4 CPUs boot via INIT-SIPI-SIPI, per-CPU data via GS segment,
per-CPU runqueue infrastructure exists (load counters with atomics), but APs idle
(`sti; hlt` loop). BSP still uses global `current_process` (231+ references).

### Steps:
1. **Per-CPU `current_process`** — Replace global with `percpu_data->current_process`.
   Macro `current` reads from GS segment. ~231 call sites to update.
2. **AP scheduler activation** — Each AP enters `schedule()` loop after boot.
   Timer interrupt on each AP drives preemption via per-CPU LAPIC timer.
3. **Load balancing** — Periodic task migration between runqueues (pull model).
   `rq_active` bitmap per CPU; idle CPU steals from busiest.
4. **IPI for reschedule** — Send IPI to wake idle APs when new process enqueued.
5. **SMP-safe process lifecycle** — `fork()`, `exit()`, `waitpid()` with proper
   locking across CPUs. Process state transitions need atomic CAS.
6. **Testing** — Stress test with multiple CPU-bound processes, verify no deadlocks.

### Risks:
- Global `current_process` refactor is the biggest risk (231+ sites).
- Race conditions in scheduler are subtle — need extensive testing.
- LAPIC timer calibration must be per-CPU.

---

## 6B. Multi-Architecture Subsystems (Priority: Medium, Effort: Very Large — 4-8 weeks)

**Current state:** ARM64, RISC-V, MIPS all boot + UART output. No PMM/VMM/scheduler/syscalls.

### Per-architecture milestones:

#### ARM64 (AArch64)
1. **PMM** — Page frame allocator using DTB memory map
2. **VMM** — 4-level page tables (TTBR0/TTBR1), kernel at upper half
3. **Exception vectors** — EL1 vector table, SVC handler for syscalls
4. **Timer** — ARM generic timer (CNTPCT_EL0)
5. **Scheduler** — Context switch via `stp`/`ldp` of callee-saved regs
6. **Syscall dispatch** — SVC #0 with register convention

#### RISC-V (RV64)
1. **PMM** — Sv39/Sv48 page tables from DTB
2. **VMM** — SATP register, kernel mapping
3. **Trap handler** — `stvec`, `scause`-based dispatch
4. **Timer** — CLINT timer interrupt (mtime/mtimecmp)
5. **Scheduler** — Context switch via `csrrw` + register save
6. **Syscall dispatch** — `ecall` from U-mode

#### MIPS32
1. **PMM** — Physical memory detection from boot arguments
2. **VMM** — TLB-based (software TLB refill handler)
3. **Exception handler** — General exception vector at 0x80000180
4. **Timer** — CP0 Count/Compare
5. **Scheduler + syscalls** — `syscall` instruction dispatch

### Approach:
- Implement one arch fully (ARM64 recommended — most useful) before starting others.
- Share as much kernel code as possible via arch-agnostic interfaces
  (`arch_process.h`, `arch_signal.h`, HAL layer).

---

## 6C. Rump Kernel Integration (Priority: Medium, Effort: Large — 4-6 weeks)

**Current state:** All prerequisites met (mutex, semaphore, condvar, threads, nanosecond
clock, IRQ chaining, PCI scanner). Roadmap documented in `RUMP_KERNEL_ROADMAP.md`.

### Stages:
1. **Phase 1-3 hypercalls** (1 week) — Memory, console, threads, sync, clocks.
   Implement `librumpuser_adros.c` with ~35 functions mapping to AdrOS primitives.
2. **Cross-compile Rump components** (1 week) — Build `librump.a`, `librumpvfs.a`,
   `librumpfs_ext2fs.a` using `buildrump.sh` with AdrOS target.
3. **Stage 1: ext2 via Rump** (1 week) — Mount ext2 image using NetBSD driver.
   Compare with native AdrOS ext2 driver for correctness.
4. **Stage 2: Network via Rump** (1-2 weeks) — Replace lwIP with NetBSD TCP/IP.
   Wire E1000 via rumpdev PCI passthrough.
5. **Stage 3: USB** (2 weeks) — xHCI/EHCI host controller + mass storage.

### Deliverables:
- USB mass storage support (major capability gap)
- Production-quality ext2/network without maintaining custom drivers

---

## 6D. Intel HDA Audio Driver (Priority: Low, Effort: Medium — 1-2 weeks)

**Current state:** PCI scanner exists, DMA infrastructure exists (circular ring buffers
for ATA and E1000). No audio support.

### Steps:
1. **PCI probe** — Detect HDA controller (class 0x0403, vendor Intel)
2. **CORB/RIRB** — Command outbound/response inbound ring buffers (DMA)
3. **Codec discovery** — Walk codec tree, find audio output widget
4. **Stream setup** — BDL (Buffer Descriptor List) for PCM output
5. **PCM playback** — `/dev/dsp` or `/dev/audio` device node, simple WAV playback
6. **Mixer** — Volume control via codec verbs

### Alternative: Use Rump kernel's HDA driver (Stage 4 in 6C) instead of writing from scratch.

---

## 6E. USTAR InitRD Format (Priority: Low, Effort: Small — 2-3 days)

**Current state:** Custom binary initrd format with LZ4 decompression. Works well but
non-standard.

### Steps:
1. **USTAR parser** — Parse 512-byte USTAR headers in initrd blob
2. **File extraction** — Create VFS entries from tar entries (files, directories, symlinks)
3. **Tool update** — Modify `tools/mkinitrd.c` to produce USTAR or keep both formats
4. **LZ4 integration** — Support `.tar.lz4` compressed USTAR archives

### Benefits:
- Standard format — can use host `tar` to create initrd
- Easier debugging — `tar tvf` to inspect contents

---

## 6F. Remaining Minor POSIX Gaps (Priority: Low, Effort: Small — 1-2 days)

### Kernel syscalls:
| Syscall | Description | Effort |
|---------|-------------|--------|
| `madvise` | Memory advice (can be no-op) | Trivial |
| `mremap` | Remap virtual memory pages | Medium |
| `execveat` | Execute relative to dirfd | Small |

### ulibc stubs:
| Feature | Description | Effort |
|---------|-------------|--------|
| `mntent` functions | Mount table parsing (`getmntent`, `setmntent`) | Small |
| `utmp`/`wtmp` | Login records (`getutent`, `pututline`) | Small |
| Full network `ioctl` | `SIOCGIFADDR`, `SIOCGIFFLAGS`, etc. | Medium |

### Type upgrades (breaking changes):
| Change | Impact | Effort |
|--------|--------|--------|
| `time_t` → 64-bit | Y2038 fix | Medium (audit all uses) |
| `off_t` → 64-bit | Large file support | Medium |
| `ssize_t` returns | POSIX compliance for read/write | Small |

---

## 6G. Bash & Busybox Ports (Priority: High, Effort: Medium — 1-2 weeks)

**Current state:** All kernel/library blockers resolved. Native toolchain (GCC 13.2 +
Binutils 2.42) and Newlib port are complete.

### Bash:
1. Cross-compile with `i686-adros-gcc` + Newlib
2. Configure with `--without-bash-malloc --disable-nls`
3. Package in initrd as `/bin/bash`
4. Test: interactive shell, pipes, redirects, job control, globbing

### Busybox:
1. Cross-compile with minimal config (start with ~20 applets)
2. Enable iteratively: `ls`, `cat`, `cp`, `mv`, `rm`, `mkdir`, `grep`, `sed`, `awk`
3. Replace individual `/bin/*` utilities with Busybox symlinks
4. Test each applet against AdrOS smoke test expectations

---

## Recommended Implementation Order

| Priority | Sub-tier | Rationale |
|----------|----------|-----------|
| 1 | **6G** Bash/Busybox | Highest user-visible impact, all blockers resolved |
| 2 | **6F** Minor POSIX gaps | Quick wins, improves compatibility |
| ~~3~~ | ~~**6A** Full SMP~~ | ✅ DONE (commit 1374a6f) |
| 4 | **6C** Rump Kernel | Unlocks USB, better drivers |
| 5 | **6E** USTAR initrd | Small, improves developer experience |
| 6 | **6D** HDA Audio | Nice-to-have |
| 7 | **6B** Multi-arch | Very large, low urgency |

---

## Summary

| Sub-tier | Description | Effort | Priority |
|----------|-------------|--------|----------|
| 6A | Full SMP scheduling | ✅ DONE | High |
| 6B | Multi-arch (ARM64/RV/MIPS) subsystems | 4-8 weeks | Medium |
| 6C | Rump Kernel integration | 4-6 weeks | Medium |
| 6D | Intel HDA audio driver | 1-2 weeks | Low |
| 6E | USTAR initrd format | 2-3 days | Low |
| 6F | Minor POSIX gaps (madvise, mntent, utmp) | 1-2 days | Low |
| 6G | Bash & Busybox ports | 1-2 weeks | High |

**Total estimated effort:** 12-22 weeks (if done sequentially; many items are parallel).
