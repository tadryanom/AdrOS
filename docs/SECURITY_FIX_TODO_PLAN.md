# Security Fix TODO Plan

## Overview
This document outlines the implementation plan for the remaining 3 items from `SECURITY_FIX_PLAN_2026-05-25.md` that require additional infrastructure or testing.

## Status Summary
- **Completed**: 23/25 items (92%)
- **Pending**: 3 items (8%)
- **Blocker**: Multi-user authentication infrastructure

---

## Pending Items

### K12/K13/K23: /proc Access Control

**Current Status**: Partially implemented, disabled due to regressions
**Location**: `src/kernel/procfs.c:42-53`

**Problem**:
- UID check implemented but disabled because AdrOS lacks complete UID/EUID infrastructure
- Processes are created with uid=0 by default
- No real authentication mechanism exists
- Check blocked access during tests even when it shouldn't

**Implementation Requirements**:

#### Phase 1: UID Infrastructure
1. **Process UID Inheritance**
   - Ensure fork/clone properly inherit uid/euid/suid/sgid
   - Verify execve doesn't reset uid to 0
   - Add tests for UID inheritance across process lifecycle

2. **Login/Authentication**
   - Implement basic login mechanism (e.g., /bin/login)
   - Add password file support (/etc/passwd, /etc/shadow)
   - Implement setuid/setgid/seteuid/setegid properly
   - Add PAM-like framework for future extensibility

3. **System Process UID Assignment**
   - init process: uid=0 (root)
   - System services: uid=0 or dedicated service UIDs
   - User processes: uid from login

#### Phase 2: /proc UID Check Implementation
1. **Re-enable UID Check in proc_find_pid_safe**
   ```c
   if (current_process && current_process->uid != p->uid && current_process->uid != 0) {
       p = NULL;  /* Access denied */
   }
   ```

2. **Add Process Pin/Refcount**
   - Implement process_ref/process_unref functions
   - Call process_ref before returning from proc_find_pid_safe
   - Call process_unref after read operation completes
   - Prevents UAF if process exits during read

3. **Address Redaction in /proc/<pid>/maps**
   - Redact physical addresses from memory maps
   - Only show virtual addresses to non-root users
   - Prevent information leak about kernel memory layout

**Testing**:
- Test that root can read all /proc/<pid>/*
- Test that non-root can only read own /proc/<pid>/*
- Test that non-root cannot read other processes' /proc/<pid>/*
- Test that process exit during read doesn't cause UAF
- Test address redaction in maps for non-root users

**Estimated Effort**: 2-3 days

---

### K15: Raw Socket Privilege Check

**Current Status**: Partially implemented, disabled due to regressions
**Location**: `src/kernel/socket.c:252-261`

**Problem**:
- Same UID infrastructure issue as K12/K13/K23
- Check blocked all socket creation when not properly configured

**Implementation Requirements**:

#### Phase 1: UID Infrastructure (Shared with K12/K13/K23)
- Same Phase 1 requirements as K12/K13/K23

#### Phase 2: Re-enable Privilege Check
1. **Re-enable Check in ksocket_create**
   ```c
   if (type == SOCK_RAW) {
       if (!current_process || current_process->uid != 0) {
           return -EPERM;
       }
   }
   ```

2. **Add CAP_NET_RAW Capability (Optional)**
   - Implement Linux-style capability framework
   - Allow non-root with CAP_NET_RAW to create raw sockets
   - More flexible than simple uid==0 check

**Testing**:
- Test that root can create SOCK_RAW sockets
- Test that non-root cannot create SOCK_RAW sockets
- Test that non-root with CAP_NET_RAW can create SOCK_RAW (if implemented)
- Test ICMP ping works as root
- Test ICMP ping fails as non-root

**Estimated Effort**: 1-2 days (depends on UID infrastructure)

---

### K24: NX Flag in Shared Memory

**Current Status**: Disabled for safety
**Location**: `src/kernel/shm.c:191`

**Problem**:
- NX bit is now properly enabled in boot.S and works for ELF loader, mmap, brk
- SHM mapping with NX needs additional testing
- Risk of breaking JIT compilers or other legitimate use cases

**Implementation Requirements**:

#### Phase 1: Test Suite
1. **Create SHM NX Test Cases**
   - Test basic SHM read/write with NX
   - Test SHM used for code storage (if applicable)
   - Test SHM used for JIT compilation
   - Test SHM used for inter-process communication

2. **Add mprotect Support for SHM**
   - Implement shmctl with IPC_RMID to change permissions
   - Allow users to explicitly enable/disable NX on SHM segments
   - Add PROT_EXEC support for SHM via mprotect

#### Phase 2: Enable NX with Fallback
1. **Enable NX by Default**
   ```c
   vmm_map_page((uint64_t)seg->pages[i],
                (uint64_t)(vaddr + i * PAGE_SIZE),
                VMM_FLAG_PRESENT | VMM_FLAG_RW | VMM_FLAG_USER | VMM_FLAG_NX);
   ```

2. **Add SHM Creation Flag for Executable SHM**
   - Add flag to shmget to request executable SHM
   - Only allow root to create executable SHM
   - Document security implications

3. **Add Runtime Detection**
   - Detect if NX causes issues (e.g., via signal handler)
   - Log warnings if process tries to execute from SHM
   - Provide diagnostic information

**Testing**:
- Run full test suite with NX enabled in SHM
- Test that SHM read/write works correctly
- Test that execution from SHM is blocked
- Test that mprotect can enable execution if needed
- Test that JIT use cases still work (if any exist)

**Estimated Effort**: 1-2 days

---

## Implementation Order

### Option 1: Complete UID Infrastructure First (Recommended)
1. Implement Phase 1 of K12/K13/K23 (UID infrastructure)
2. Complete K12/K13/K23 (Phase 2)
3. Complete K15 (Phase 2)
4. Complete K24

**Advantages**:
- Solves infrastructure dependency once
- Enables both K12/K13/K23 and K15
- More comprehensive security model

**Disadvantages**:
- Larger upfront effort
- Longer time to see results

### Option 2: Quick Wins First
1. Complete K24 (independent of UID infrastructure)
2. Implement minimal UID infrastructure
3. Complete K12/K13/K23
4. Complete K15

**Advantages**:
- K24 can be done independently
- Faster initial progress

**Disadvantages**:
- UID infrastructure still needed for K12/K13/K23 and K15
- May need to revisit UID infrastructure design

---

## Dependencies

```
K12/K13/K23 ──┐
              ├──> UID Infrastructure (Phase 1)
K15 ──────────┘

K24 (independent)
```

---

## Testing Strategy

### Unit Tests
- UID inheritance tests
- /proc access control tests
- Socket privilege tests
- SHM NX tests

### Integration Tests
- Multi-user login/logout flow
- Root vs non-root process behavior
- Cross-process SHM with NX

### Regression Tests
- Ensure existing tests still pass
- No performance degradation
- No new kernel panics

---

## Risk Assessment

### High Risk
- UID infrastructure changes affect entire system
- May break existing functionality
- Requires extensive testing

### Medium Risk
- /proc access control may break monitoring tools
- Socket privilege check may break network tools

### Low Risk
- SHM NX flag is isolated to SHM subsystem
- Can be easily disabled if issues arise

---

## Success Criteria

- All 3 items implemented and enabled
- Test suite passes with no regressions
- Documentation updated
- Security audit passes

---

## Timeline Estimate

- **UID Infrastructure**: 3-4 days
- **K12/K13/K23**: 2-3 days
- **K15**: 1-2 days
- **K24**: 1-2 days
- **Testing & Validation**: 2-3 days

**Total**: 9-14 days (depending on option chosen)
