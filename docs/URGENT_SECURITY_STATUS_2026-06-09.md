# Urgent Security Status - 2026-06-09

## Objetivo

Este documento substitui a checklist antiga de fases 2-5 como referência operacional do estado atual do AdrOS.

A lista anterior ficou parcialmente desatualizada: vários itens já foram implementados, alguns ficaram parcialmente resolvidos e ainda restam gaps reais que merecem ataque prioritário.

Este documento foi construído com base em verificação direta do código-fonte atual.

## Resumo Executivo

### Já implementado

- SOCK_RAW privilege check
- rumpuser_free alignment handling
- VFS permission checks principais
- execve com verificação de bit executável
- O_NOFOLLOW retornando -ELOOP
- truncate/ftruncate sem fallback permissivo
- fb0_mmap respeitando prot e NX
- mkstemp com retry em colisão
- fcntl varargs em ulibc
- fcntl/openat varargs em newlib/libgloss
- scanf com suporte a field width
- grande parte da infraestrutura de UID/GID no kernel
- SHM com uid/gid/mode básicos e NX por default
- path_resolve_user/copy_user_cstr retornando ENAMETOOLONG em caminhos importantes

### Parcialmente implementado

- AIO validation
- /proc access control
- SHM permissions completas
- futex robusto
- tmpfs locking/quotas/overflow hardening
- overlayfs wrapper lifetime
- socket copy_to_user hardening
- POSIX access() estrito
- infraestrutura UID completa de userspace/sessão
- padronização total de ENAMETOOLONG

### Ainda pendente de verdade

- remover fixed VAs (KVA_PHYS_MAP / VIRTIO_VRING_VA)
- futex robusto com locking e shared keying correto
- /proc hardening completo
- AIO mode/mount validation completa
- tmpfs quotas e proteção explícita contra overflow no growth path
- CSPRNG real
- getlogin()/who/utmp reais
- revisão final dos copyouts de socket

## Classificação Atual por Item

## Fase 2 - Isolamento e Políticas de Segurança

### H1: AIO fd Mode e Read-Only Mount Validation
Status: PARCIAL

Evidência atual:
- `src/kernel/syscall.c`: `syscall_aio_rw_impl()` valida ponteiros, tamanho e usa bounce buffer.

O que falta:
- rejeitar `aio_read` em fd `O_WRONLY`
- rejeitar `aio_write` em fd `O_RDONLY`
- rejeitar escrita em mount `MS_RDONLY`
- idealmente reutilizar a mesma política usada por `read/write/pread/pwrite`

Prioridade recomendada: ALTA

### H2: /proc Access Control
Status: PARCIAL

Evidência atual:
- `src/kernel/procfs.c`: existe `proc_access_check(pid)`
- `/proc/<pid>/status`, `/proc/<pid>/maps`, `/proc/<pid>/cmdline` usam esse check

O que falta:
- proteger `/proc/dmesg`
- proteger `/proc/cmdline`
- implementar hidepid ou equivalente
- redigir endereços em `/proc/<pid>/maps` para não-root
- revisar lifetime/snapshot dos dados lidos

Prioridade recomendada: ALTA

### H3: SHM Permissões Completas
Status: PARCIAL

Evidência atual:
- `src/kernel/shm.c` contém `uid`, `gid`, `mode`
- `shm_perm_check()` já usa owner/group/other
- `IPC_STAT`, `IPC_SET`, `IPC_RMID` fazem checagens
- mapeamento aplica `VMM_FLAG_NX`

O que falta:
- attach read-only no estilo `SHM_RDONLY`
- revisar completude da modelagem SysV/POSIX
- checar se faltam casos de permissão e compatibilidade fina

Prioridade recomendada: MÉDIA

### H4: SOCK_RAW Privilege Check
Status: IMPLEMENTADO

Evidência atual:
- `src/kernel/socket.c`: `SOCK_RAW` exige `current_process->euid == 0`

Prioridade recomendada: FECHADO

### H5: Futex Robusto com Spinlock e Shared Memory Keying
Status: PARCIAL

Evidência atual:
- `src/kernel/syscall.c`: waiters registram `addr`, `addr_space`, `proc`
- isso evita parte do aliasing entre address spaces diferentes

O que falta:
- spinlock protegendo a tabela global
- chaveamento shared por backing físico ou inode+offset
- coordenação mais robusta entre wait/wake e scheduler
- revisão SMP da tabela e wakeup paths

Prioridade recomendada: ALTA

### H6: Remover Fixed VAs
Status: PENDENTE

Evidência atual:
- `src/hal/x86/mm.c`: ainda usa `KVA_PHYS_MAP`
- `src/drivers/virtio_blk.c`: ainda usa `VIRTIO_VRING_VA 0xC0340000U`

O que falta:
- alocador de VA kernel para MMIO / DMA / mapeamentos temporários
- remoção de VAs fixas locais
- revisão de colisões potenciais com outros dispositivos

Prioridade recomendada: ALTA

### H7: Tmpfs Locking, Quotas e Overflow Validation
Status: PARCIAL

Evidência atual:
- `src/kernel/tmpfs.c`: existe `g_tmpfs_lock`
- várias operações usam spinlock

O que falta:
- quota global
- validação explícita de overflow no crescimento de `new_cap *= 2`
- revisar metadados `uid/gid/mode` e consistência completa

Prioridade recomendada: ALTA

### H8: rumpuser_free Alignment Handling
Status: IMPLEMENTADO

Evidência atual:
- `src/rump/rumpuser_adros.c`: aligned allocations guardam magic + raw pointer
- `rumpuser_free()` recupera o ponteiro bruto antes do `kfree`

Prioridade recomendada: FECHADO

## Fase 3 - Polimento de VFS e Userspace

### M1: VFS Permissions / execve Bit Check
Status: IMPLEMENTADO

Evidência atual:
- `src/kernel/fs.c`: `vfs_check_permission()` usa uid/gid/euid/egid
- `src/kernel/syscall.c`: `execve` verifica execute permission com `vfs_check_permission(node, 1)`

Prioridade recomendada: FECHADO

### M2: POSIX access() Completo
Status: PARCIAL

Evidência atual:
- `src/kernel/syscall.c`: `F_OK/R_OK/W_OK/X_OK` já são tratados

O que falta:
- usar uid/gid reais, não efetivos, para aderência POSIX estrita

Prioridade recomendada: MÉDIA

### M3: O_NOFOLLOW -ELOOP Return
Status: IMPLEMENTADO

Evidência atual:
- `src/kernel/syscall.c`: final symlink com `O_NOFOLLOW` retorna `-ELOOP`

Prioridade recomendada: FECHADO

### M4: Truncate Fallback Remoção
Status: IMPLEMENTADO

Evidência atual:
- `src/kernel/fs.c`: `vfs_truncate()` e `vfs_truncate_node()` retornam `-ENOSYS` sem backend

Prioridade recomendada: FECHADO

### M5: Socket copy_to_user Ignored Failures
Status: PARCIAL

Evidência atual:
- `src/kernel/syscall.c`: vários caminhos de socket já verificam `copy_to_user`
- `recv`, `recvfrom`, `recvmsg`, `getsockname`, `getpeername`, `accept` foram endurecidos parcialmente

Risco remanescente:
- alguns paths parecem usar condição invertida com `user_range_ok(...) == 0` antes do `copy_to_user`
- isso precisa revisão manual e correção pontual

Prioridade recomendada: ALTA

### M6: Overlayfs Wrapper Lifetime/Refcount
Status: PARCIAL

Evidência atual:
- `src/kernel/overlayfs.c`: wrapper nodes têm `refcount`
- `overlay_wrap_close()` decrementa e libera

O que falta:
- revisão de ownership/lifetime em todos os caminhos de uso
- confirmar ausência de wrappers órfãos ou double-free/UAF em cenários complexos

Prioridade recomendada: MÉDIA

### M7: fb0_mmap Respeitar prot e NX
Status: IMPLEMENTADO

Evidência atual:
- `src/drivers/vbe.c`: rejeita `PROT_EXEC`, aplica `VMM_FLAG_NX`, respeita `prot`

Prioridade recomendada: FECHADO

### M8: CSPRNG Implementation
Status: PENDENTE

Evidência atual:
- `src/kernel/devfs.c`: `/dev/random` e `/dev/urandom` usam PRNG simples, não criptográfico
- `src/kernel/kaslr.c`: usa xorshift32 seeded from TSC

O que falta:
- fonte real de entropia
- CSPRNG central de kernel
- consumidores do sistema apontando para essa fonte

Prioridade recomendada: ALTA

### M9: mkstemp Retry Multiple Names
Status: IMPLEMENTADO

Evidência atual:
- `user/ulibc/src/stdlib.c`: tenta até 100 vezes em caso de `EEXIST`

Prioridade recomendada: FECHADO

### M10: fcntl Varargs em ulibc
Status: IMPLEMENTADO

Evidência atual:
- `user/ulibc/src/unistd.c`: vararg só é lido para comandos que precisam

Prioridade recomendada: FECHADO

### M11: newlib libgloss Varargs
Status: IMPLEMENTADO

Evidência atual:
- `newlib/libgloss/adros/posix_stubs.c`: `fcntl` e `openat` já fazem leitura condicional

Prioridade recomendada: FECHADO

### M12: scanf Field Width e Buffer Size
Status: IMPLEMENTADO

Evidência atual:
- `user/ulibc/src/stdio.c`: `scanf`, `sscanf` e `fscanf` tratam field width explícita

Prioridade recomendada: FECHADO

## Fase 4 - Infraestrutura UID

### UID Infrastructure Completa
Status: PARCIAL

Evidência atual:
- kernel já possui `getuid/getgid/geteuid/getegid`
- kernel já possui `setuid/setgid/seteuid/setegid/setreuid/setregid`
- fork/clone herdam `uid/gid/euid/egid/suid/sgid`
- job control, sessão e pgrp já existem no kernel

O que falta:
- `/etc/passwd` real
- autenticação/login real
- integração com utmp/wtmp
- `getlogin()` real
- `who` real
- stubs de passwd/group ainda são hardcoded em partes do userspace/newlib

Prioridade recomendada: MÉDIA

## Fase 5 - Low Priority

### L1: Path Truncation - ENAMETOOLONG
Status: PARCIAL AVANÇADO

Evidência atual:
- `src/kernel/syscall.c`: `copy_user_cstr()` retorna `-ENAMETOOLONG`
- `src/kernel/syscall.c`: `path_resolve_user()` retorna `-ENAMETOOLONG`

O que falta:
- padronizar o mesmo comportamento em todos os helpers de path em `fs.c`
- alguns caminhos ainda retornam `-EINVAL` ou operam com truncamento em buffers fixos

Prioridade recomendada: MÉDIA-BAIXA

### L2: getlogin/who Session/TTY/utmp
Status: PENDENTE

Evidência atual:
- `user/ulibc/src/unistd.c`: `getlogin()` retorna fallback fixo
- `user/cmds/who/who.c`: saída fixa placeholder
- `user/ulibc/src/utmp.c`: implementação stub

Prioridade recomendada: BAIXA

## Lista Urgente Recomendada

## Prioridade 1 - Atacar já

- H1: AIO validation completa
- H2: /proc hardening completo
- H5: futex robusto
- H6: remover fixed VAs
- H7: tmpfs quotas + overflow hardening
- M5: revisão final dos socket copyouts
- M8: CSPRNG real

## Prioridade 2 - Em seguida

- H3: SHM permissões/comportamento completos
- M2: access() com real IDs
- M6: overlayfs wrapper lifetime
- Fase 4 restante de userspace/login/passwd
- L1: padronização total de ENAMETOOLONG

## Prioridade 3 - Baixa

- L2: getlogin/who/utmp reais

## Conclusão

A checklist antiga não deve mais ser usada como retrato fiel do estado atual do projeto.

O estado real hoje é:

- muitos itens relevantes já foram corrigidos
- alguns itens centrais ainda estão parciais
- os gaps urgentes se concentram em isolamento, concorrência e política de segurança:
  - AIO policy
  - /proc hardening
  - futex robusto
  - fixed VA removal
  - tmpfs hardening
  - CSPRNG
  - revisão final dos copyouts de socket

## Status do documento

Documento urgente criado para substituir a checklist desatualizada como referência de planejamento imediato.
