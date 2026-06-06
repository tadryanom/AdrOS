# AdrOS Rootfs Handoff Plan

This document describes a practical roadmap to evolve AdrOS from its current permanent `initrd + overlayfs` root model toward a more conventional Unix-like boot flow where the real disk filesystem becomes `/` during normal boot.

## 1. Current State

Today AdrOS boots with the following model:

- The kernel loads the `initrd` and initializes `fs_root` from it.
- The kernel creates a `tmpfs` upper layer and mounts `overlayfs(initrd, tmpfs)` on `/`.
- The kernel mounts fallback virtual filesystems such as `/tmp`, `/dev`, and `/proc`.
- If `root=` is provided, AdrOS currently auto-detects `ext2` or `fat` and mounts the selected device on `/disk`.
- Userspace `/sbin/init` only mounts missing virtual filesystems and parses `/etc/fstab` for extra mounts.
- No normal boot-time `switch_root` or `pivot_root` handoff occurs.

As a result, the `initrd` remains the effective permanent root filesystem during a normal boot, while the real disk root is only available as an auxiliary mount.

## 2. Target State

AdrOS should move toward a boot flow closer to Linux or BSD systems:

- `root=` should identify the final root filesystem.
- The real disk filesystem should become `/` during normal boot.
- The `initrd` should be temporary in the common boot path, or reserved for rescue/special-purpose boot modes.
- `/sbin/init` should run from the real root, not from the `initrd` overlay root.
- `/etc/fstab` should only manage follow-up mounts after the real root is active.

## 3. Recommended Direction

The least disruptive path is a Linux-like model:

1. Keep `initrd` support.
2. Introduce an early userspace `/init` inside the `initrd`.
3. Mount the real root on a staging directory such as `/newroot`.
4. Perform `pivot_root` or `switch_root` during normal boot.
5. Execute `/sbin/init` from the new root.

This is likely easier than immediately moving to a BSD-style direct kernel `mountroot` flow because AdrOS already has `initrd`, `pivot_root`, and the basic machinery needed to mount disk filesystems before the final handoff.

## 4. Implementation Milestones

### Milestone A — Redefine `root=` Semantics

Goal:
- Make `root=` identify the final root filesystem rather than an automatic mount target at `/disk`.

Tasks:
- Change boot semantics so that `root=` means the intended final root device or partition.
- Preserve existing device resolution for `/dev/hdX` and `/dev/hdXn`.
- Mount the detected root filesystem on a staging path such as `/newroot` instead of `/disk`.
- Keep `/disk` only as an optional secondary/manual mountpoint if it remains useful.

Expected result:
- Boot logic can resolve and stage the real root without yet replacing `/`.

### Milestone B — Add Early Userspace `/init`

Goal:
- Move root handoff policy into a dedicated early boot program.

Tasks:
- Add a small `/init` program to the `initrd`.
- Make `/init` responsible for early boot tasks related to the real root.
- Resolve the root device passed by `root=`.
- Mount the real root under `/newroot`.
- Prepare required directories and prerequisites for the handoff.

Expected result:
- Early userspace is now explicitly responsible for transitioning from temporary boot environment to real system root.

### Milestone C — Integrate `pivot_root` / `switch_root`

Goal:
- Make the real root become `/` in the normal boot flow.

Tasks:
- Move, recreate, or remount essential mounts such as `/dev`, `/proc`, `/tmp`, and later `/run` under the new root.
- Perform the root handoff using `pivot_root` or a `switch_root`-style wrapper.
- Ensure the old root is no longer the active root during normal operation.
- `execve()` the real `/sbin/init` from the new root.

Expected result:
- AdrOS boots into the real disk filesystem as `/`.

### Milestone D — Adapt `/sbin/init` and `fstab`

Goal:
- Make post-handoff userspace behavior match the new root model.

Tasks:
- Ensure `/sbin/init` assumes it is already running on the final root filesystem.
- Keep virtual filesystem mounting logic only for missing mounts that truly belong in normal userspace init.
- Treat `/etc/fstab` as a source of non-root follow-up mounts.
- Avoid redoing work already completed by early `/init` or the handoff path.

Expected result:
- `/sbin/init` becomes the normal system init, not the handoff coordinator.

### Milestone E — Add Boot Policy and Hardening

Goal:
- Bring the root boot path closer to established Unix-like systems.

Tasks:
- Add support for `rootfstype=`.
- Add support for `rootflags=`.
- Add support for `ro` / `rw` root boot modes.
- Add support for `rootwait` and possibly `rootdelay`.
- Later add a policy for root verification and `remount,rw` after checks.
- Consider future support for identifiers such as `UUID=` or `LABEL=`.

Expected result:
- The root boot path becomes more robust, configurable, and closer to Linux/BSD expectations.

## 5. Optional BSD-like Alternative

A future alternative is a more BSD-like approach:

- The kernel resolves the final root directly.
- The kernel mounts the real root on `/` itself.
- The kernel then starts `/sbin/init` already on the final root.

This remains a valid architectural direction, but it is likely more disruptive than the Linux-like staged handoff because AdrOS already has strong building blocks for an `initrd -> /newroot -> switch_root` model.

## 6. Architectural End Goal

The main architectural objective is:

- During normal boot, the real disk-backed filesystem becomes `/`.
- The `initrd` stops being the effective permanent root.
- `root=` gains the expected meaning of “final system root”.
- `/sbin/init` executes from the final root and `/etc/fstab` handles only subsequent mounts.

## 7. Summary

AdrOS already has enough infrastructure to start this transition incrementally. The most practical sequence is:

1. Stage the real root at `/newroot`.
2. Add a dedicated early `/init`.
3. Perform `pivot_root` / `switch_root` in the standard boot path.
4. Run the final `/sbin/init` from the real root.
5. Add boot-policy refinements such as `rootfstype`, `rootflags`, `ro/rw`, and `rootwait`.

This keeps the implementation incremental while moving AdrOS toward a more conventional and scalable root filesystem boot design.
