# Security Fix Plan - 2026-05-26

## Contexto

Este plano é baseado na reanálise completa do AdrOS (ZIP CDFA4C08) realizada em 2026-05-25, cruzada com as correções já implementadas em sessões anteriores (documentadas nas memórias do sistema).

## Status dos Itens do Relatório

### Itens JÁ Corrigidos (sessões anteriores)

| ID | Descrição | Status |
|---|---|---|
| A16 | pmm_decref underflow check | ✅ Corrigido |
| A03 | pread/pwrite mode validation | ✅ Corrigido |
| A04 | O_RDONLY\|O_TRUNC rejeição | ✅ Corrigido |
| A14 | truncate/ftruncate VFS integration | ✅ Corrigido |
| A17 | varargs open/openat/fcntl | ✅ Corrigido |
| U04 | execl/execlp varargs | ✅ Corrigido |
| U02 | scanf %s limit (255 bytes) | ✅ Corrigido |
| A18 | shell command substitution | ✅ Corrigido |
| A13 | posix_spawn PID fix | ✅ Corrigido |
| A19 | access() mode implementation | ✅ Corrigido |
| K21 | O_NOFOLLOW implementation | ✅ Corrigido |
| K35 | socket bounce buffers (SMAP) | ✅ Corrigido |
| K14 | SHM permissions | ✅ Corrigido |
| K22 | dlopen per-process isolation | ✅ Corrigido |
| K17 | futex keying (mm, uaddr) | ⚠️ Parcial (addr_space adicionado, mas não robusto) |
| K24 | NX flag in SHM | ⚠️ TODO (desabilitado temporariamente) |

### Itens Tentados mas Causaram Regressões

| ID | Descrição | Status |
|---|---|---|
| K12/K13/K23 | /proc access control (UID checks) | ❌ Requer infraestrutura UID completa |
| K15 | raw socket privilege (UID check) | ❌ Requer infraestrutura UID completa |

### Itens Remanescentes (não abordados)

Abaixo estão os itens que ainda precisam ser corrigidos, organizados por prioridade.

---

## Fase 1: Críticos de Memória e Parser (7 itens)

### C1: ELF Loader p_filesz > p_memsz Validation
**Severidade:** CRÍTICA  
**Arquivos:** `src/arch/x86/elf.c`  
**Problema:** O loader valida `p_offset + p_filesz <= file_len` mas não valida `p_filesz <= p_memsz`. Isso permite escrever além da área mapeada.  
**Solução:**
- Adicionar validação `if (ph[i].p_filesz > ph[i].p_memsz) return -EINVAL;` em `elf32_load_segments`
- Aplicar mesma validação em `elf32_load_shared_lib_at` (dlopen path)
- Validar também `p_align`, `e_phentsize`, `e_phnum`, tipo ELF, endianess e ABI

### C2: NX por Default em Todos os Mapeamentos User
**Severidade:** CRÍTICA  
**Arquivos:** `src/kernel/syscall.c`, `src/arch/x86/elf.c`, `src/kernel/shm.c`, `src/drivers/vbe.c`  
**Problema:** NX não é aplicado por default em mmap anônimo, brk, stack inicial, SHM e fb0_mmap.  
**Solução:**
- `mmap`: Adicionar `VMM_FLAG_NX` por default, remover apenas se `PROT_EXEC` presente
- `brk`: Adicionar `VMM_FLAG_NX` ao criar heap
- `elf32_load_user_from_initrd`: Stack inicial deve ter NX
- `shm_at`: Mapear com NX por default (já documentado como TODO K24)
- `fb0_mmap`: Aplicar NX e respeitar argumento `prot`

### C3: Initrd TAR Parser com Limites de Tamanho
**Severidade:** CRÍTICA  
**Arquivo:** `src/drivers/initrd.c`  
**Problema:** Loop TAR sem limite pelo tamanho do initrd, `tar_is_zero_block` sem validar 512 bytes disponíveis, acesso a header/payload sem bounds checking.  
**Solução:**
- Manter `end = base + size` e validar cada acesso
- Validar `p + TAR_BLOCK <= end` antes de `tar_is_zero_block`
- Validar `p + sizeof(tar_header_t) + payload <= end` antes de ler
- Validar checksum TAR e overflow em `adv`
- Validar `LZ4B_HDR_SIZE + comp_sz <= size`

### C4: pmm_boot.c Multiboot2 Parsing com Cursor/Limit
**Severidade:** CRÍTICA  
**Arquivo:** `src/arch/x86/pmm_boot.c`  
**Problema:** Loops sem validação de `total_size`, `tag->size >= 8`, limite do buffer, nem `mmap->entry_size`.  
**Solução:**
- Portar lógica de cursor/limite de `arch_early_setup.c` para `pmm_boot.c`
- Validar `tag->size >= 8` antes de acessar campos
- Validar `cursor + tag->size <= end` antes de `MB2_TAG_NEXT`
- Validar `mmap->entry_size` e bounds do array de entradas

### C5: VMA Table per Process para mprotect Robusto
**Severidade:** CRÍTICA  
**Arquivos:** `src/kernel/syscall.c`, `include/process.h`  
**Problema:** `mprotect` tem fallback permissivo para endereços não modelados como VMA.  
**Solução:**
- Criar tabela VMA unificada por processo (struct vma_region com start, end, prot)
- Registrar cada mmap, brk, stack, ELF segment na tabela VMA
- `mprotect` deve aceitar apenas ranges completamente cobertos por VMAs
- Remover fallback permissivo

### C6: Ext2 Superblock/GDT Validation
**Severidade:** CRÍTICA  
**Arquivo:** `src/kernel/ext2.c`  
**Problema:** `s_blocks_per_group`, `s_inodes_per_group` podem ser zero; `num_groups` e `gdt_bytes` podem overflowar; validação incompleta.  
**Solução:**
- Validar `s_blocks_per_group > 0` e `s_inodes_per_group > 0`
- Validar overflow em `num_groups = (s_blocks_count - s_first_data_block + s_blocks_per_group - 1) / s_blocks_per_group`
- Validar `gdt_bytes = num_groups * sizeof(ext2_group_desc_t)` não overflow
- Validar `block_size` é potência de 2 e >= 1024
- Validar inode size, GDT blocks e bitmap blocks contra limites do dispositivo

### C7: FAT BPB Validation
**Severidade:** CRÍTICA  
**Arquivo:** `src/kernel/fat.c`  
**Problema:** `total_sectors - (data_lba - partition_lba)` pode underflowar; `sectors_per_cluster` não validado como potência de 2; cadeias de cluster sem limite.  
**Solução:**
- Validar `data_lba >= partition_lba` antes de subtração
- Validar `sectors_per_cluster` é potência de 2 entre 1 e 128
- Validar `fat_size`, `reserved_sectors`, `root_cluster` contra limites do dispositivo
- Limitar caminhada de cluster por `total_clusters` em `fat_extend_chain`/`fat_free_chain`
- Detectar ciclos em cadeias de cluster

---

## Fase 2: Isolamento e Políticas de Segurança (8 itens)

### H1: AIO fd Mode e Read-Only Mount Validation
**Severidade:** ALTA  
**Arquivo:** `src/kernel/syscall.c`  
**Problema:** `syscall_aio_rw_impl` não reutiliza política de read/write/pread/pwrite.  
**Solução:**
- Centralizar validação de modo e mount em helper usado por todas as rotas de I/O
- `aio_write` deve rejeitar fd `O_RDONLY` e mount `MS_RDONLY`
- `aio_read` deve rejeitar fd `O_WRONLY`

### H2: /proc Access Control
**Severidade:** ALTA  
**Arquivo:** `src/kernel/procfs.c`  
**Problema:** Entradas `/proc/dmesg`, `/proc/cmdline`, `/proc/<pid>/status`, `/proc/<pid>/maps`, `/proc/<pid>/cmdline` sem controle de acesso.  
**Solução:**
- Implementar função `proc_may_access(pid, uid)` similar a `ptrace_may_access`
- Redigir endereços em `/proc/<pid>/maps` para não-root
- Implementar hidepid (2 = hide all pids from non-root)
- Usar pin/refcount ou snapshot sob lock para evitar UAF
- **NOTA:** Requer infraestrutura UID completa (ver Fase 4)

### H3: SHM Permissões Completas
**Severidade:** ALTA  
**Arquivo:** `src/kernel/shm.c`  
**Problema:** Falta mode por segmento, grupos, attach read-only, NX default.  
**Solução:**
- Adicionar campo `mode`, `gid` em `struct shm_segment`
- Implementar attach read-only (sem flag SHM_RDONLY no momento)
- Aplicar NX por default (já documentado como TODO K24)
- Modelar uid/gid/mode como SysV/POSIX SHM

### H4: SOCK_RAW Privilege Check
**Severidade:** ALTA  
**Arquivo:** `src/kernel/socket.c`  
**Problema:** `ksocket_create` aceita `SOCK_RAW` sem checar `current_process->euid`.  
**Solução:**
- Adicionar verificação `if (type == SOCK_RAW && current_process->euid != 0) return -EPERM`
- **NOTA:** Requer infraestrutura UID completa (ver Fase 4)

### H5: Futex Robusto com Spinlock e Shared Memory Keying
**Severidade:** ALTA  
**Arquivo:** `src/kernel/syscall.c`  
**Problema:** Tabela global sem lock; futex em memória compartilhada não usa objeto físico/inode+offset.  
**Solução:**
- Adicionar spinlock para proteger tabela global de futex
- Chavear futex privado por `(mm, uaddr)` (já parcialmente feito)
- Chavear futex compartilhado por página física ou inode+offset
- Coordenar wait/wake com scheduler de forma mais robusta

### H6: Remover Fixed VAs (KVA_PHYS_MAP)
**Severidade:** ALTA  
**Arquivos:** `src/hal/x86/mm.c`, `src/drivers/virtio_blk.c`, `include/arch/x86/kernel_va_map.h`  
**Problema:** `hal_mm_map_physical_range` usa sempre `KVA_PHYS_MAP` sem alocador; colisão virtio-blk/E1000 em VAs fixas.  
**Solução:**
- Criar alocador de VA kernel para MMIO/DMA/mapeamentos temporários
- Remover VAs fixas locais (KVA_PHYS_MAP)
- Resolver colisão virtio-blk (0xC0340000) com E1000 (0xC0330000..0xC034FFFF)

### H7: Tmpfs Locking, Quotas e Overflow Validation
**Severidade:** ALTA  
**Arquivo:** `src/kernel/tmpfs.c`  
**Problema:** Sem locks, sem quotas, `new_cap *= 2` pode virar zero, metadados não inicializados.  
**Solução:**
- Adicionar lock por árvore/inode
- Adicionar quota global
- Validar overflow em `new_cap *= 2`
- Inicializar `uid/gid/mode` corretamente

### H8: rumpuser_free Alignment Handling
**Severidade:** ALTA  
**Arquivo:** `src/rump/rumpuser_adros.c`  
**Problema:** Quando `alignment > 16`, `rumpuser_free` chama `kfree(mem)` diretamente em vez do ponteiro bruto.  
**Solução:**
- Armazenar header uniforme para todas as alocações rumpuser
- Recuperar ponteiro bruto antes de chamar `kfree`

---

## Fase 3: Polimento de VFS e Userspace (12 itens)

### M1: VFS Permissions - mode != 0 e execve Bit Check
**Severidade:** MÉDIA  
**Arquivos:** `src/kernel/fs.c`, `src/kernel/syscall.c`  
**Problema:** `vfs_check_permission` retorna sucesso quando `node->mode == 0`; `execve` não verifica bit executável.  
**Solução:**
- Remover caso permissivo quando `mode == 0`
- `execve` deve verificar bit executável do inode (S_IXUSR|S_IXGRP|S_IXOTH)

### M2: POSIX access() Completo
**Severidade:** MÉDIA  
**Arquivo:** `src/kernel/syscall.c`  
**Problema:** `access()` simplificado - `R_OK` assume legível se existir, `X_OK` testa apenas arquivo regular.  
**Solução:**
- Implementar `F_OK/R_OK/W_OK/X_OK` sobre mesma função de permissão
- Usar uid/gid reais em vez de efetivos

### M3: O_NOFOLLOW -ELOOP Return
**Severidade:** MÉDIA  
**Arquivos:** `src/kernel/syscall.c`, `src/kernel/fs.c`  
**Problema:** `vfs_lookup_nofollow` evita seguir symlink mas não retorna erro POSIX claro quando componente final é symlink.  
**Solução:**
- Se `O_NOFOLLOW` presente e componente final for `FS_SYMLINK`, retornar `-ELOOP`

### M4: Truncate Fallback Remoção
**Severidade:** MÉDIA  
**Arquivo:** `src/kernel/fs.c`  
**Problema:** `vfs_truncate` altera apenas `node->length` se backend não implementa truncate.  
**Solução:**
- Sem `i_ops->truncate`, retornar `-ENOSYS`
- Limitar fallback a filesystems explicitamente marcados como seguros (se necessário)

### M5: Socket copy_to_user Ignored Failures
**Severidade:** MÉDIA  
**Arquivo:** `src/kernel/syscall.c`  
**Problema:** `accept`, `recvfrom`, `recvmsg`, `getsockname`, `getpeername` usam `(void)copy_to_user(...)`.  
**Solução:**
- Retornar `-EFAULT` quando copyout obrigatório falhar
- Para campos opcionais, validar ponteiro e tamanho antes

### M6: Overlayfs Wrapper Lifetime/Refcount
**Severidade:** MÉDIA  
**Arquivo:** `src/kernel/overlayfs.c`  
**Problema:** `overlay_wrap_child` aloca wrappers sem caminho claro de liberação.  
**Solução:**
- Introduzir cache/refcount/release para wrappers
- Ou simplificar overlayfs até haver ownership claro

### M7: fb0_mmap Respeitar prot e NX
**Severidade:** MÉDIA  
**Arquivo:** `src/drivers/vbe.c`  
**Problema:** `fb0_mmap` ignora argumento `prot` e sempre mapeia RW sem NX.  
**Solução:**
- Respeitar `PROT_READ/PROT_WRITE`
- Negar execução e aplicar NX

### M8: CSPRNG Implementation
**Severidade:** MÉDIA  
**Arquivos:** `src/kernel/devfs.c`, `include/net/lwipopts.h`, `user/ulibc/src/rand.c`, `src/rump/rumpuser_adros.c`  
**Problema:** `/dev/random`, `/dev/urandom`, lwIP, libc usam RNGs previsíveis.  
**Solução:**
- Implementar CSPRNG de kernel com seed de entropia real (RDRND se disponível, ou TSC + timer)
- Fazer lwIP/devfs/userspace consumirem essa fonte

### M9: mkstemp Retry Multiple Names
**Severidade:** MÉDIA  
**Arquivo:** `user/ulibc/src/stdlib.c`  
**Problema:** `mkstemp` tenta apenas um nome; se colidir, falha.  
**Solução:**
- Após corrigir CSPRNG, fazer `mkstemp` tentar múltiplos nomes (loop com limite)
- Documentar/desencorajar `tmpnam`

### M10: fcntl Varargs em ulibc
**Severidade:** MÉDIA  
**Arquivo:** `user/ulibc/src/unistd.c`  
**Problema:** `fcntl` lê vararg para `F_GETFD` e `F_GETFL` que não exigem argumento.  
**Solução:**
- Ler vararg somente nos comandos que precisam: `F_SETFD`, `F_SETFL`, `F_DUPFD`, `F_DUPFD_CLOEXEC`

### M11: newlib libgloss Varargs
**Severidade:** MÉDIA  
**Arquivo:** `newlib/libgloss/adros/posix_stubs.c`  
**Problema:** `fcntl` e `openat` leem vararg incondicionalmente.  
**Solução:**
- Aplicar mesma correção que ulibc: leitura condicional de vararg

### M12: scanf Field Width e Buffer Size
**Severidade:** MÉDIA  
**Arquivo:** `user/ulibc/src/stdio.c`  
**Problema:** Limite 255 bytes não resolve API - `%s` não sabe tamanho real do buffer.  
**Solução:**
- Respeitar largura de campo (ex: `%255s`)
- Suportar formatos de largura corretamente
- Auditar comandos que usam `%s`

---

## Fase 4: Infraestrutura UID (Pré-requisito para H2, H4)

### UID Infrastructure
**Severidade:** ALTA (pré-requisito)  
**Descrição:** AdrOS é um OS single-user sem infraestrutura de autenticação multi-usuário completa.  
**Solução:**
- Implementar `/etc/passwd` parsing
- Implementar login com autenticação
- UID/EUID inheritance em fork/clone
- Per-process mount namespaces (opcional, mas desejável)
- **Estimativa:** 3-4 dias de trabalho

---

## Fase 5: Low Priority (2 itens)

### L1: Path Truncation - ENAMETOOLONG
**Severidade:** BAIXA  
**Arquivos:** `src/kernel/syscall.c`, `src/kernel/fs.c`  
**Problema:** `path_resolve_user` e `vfs_lookup_parent` truncam strings silenciosamente.  
**Solução:**
- Tratar truncamento como erro em todos os caminhos de path
- Retornar `-ENAMETOOLONG` quando input excede limite

### L2: getlogin/who Session/TTY/utmp
**Severidade:** BAIXA  
**Arquivos:** `user/ulibc/src/unistd.c`, `user/cmds/who/who.c`  
**Problema:** `getlogin()` usa fallback `"root"`, `who` imprime linha fixa.  
**Solução:**
- Conectar login a sessões/TTY/utmp
- Retornar erro quando não houver informação confiável

---

## Itens NÃO Incluídos (Falsos Positivos ou Fora de Escopo)

### sysenter.S (A02)
**Status:** NÃO incluído neste plano  
**Motivo:** Relatório menciona problema mas validação atual (`ECX < 0xC0000000` e `ECX >= 8`) é razoável para ABI sysenter. Melhoria seria mudança de ABI, não correção de bug.

### posix_spawn Kernel Implementation (A13)
**Status:** NÃO incluído neste plano  
**Motivo:** Wrapper userspace foi corrigido. Implementação kernel como fork+execve é padrão POSIX e funciona corretamente.

---

## Ordem de Execução Sugerida

1. **Fase 1 (Críticos):** 7 itens - estimativa 10-14 dias
2. **Fase 4 (UID Infrastructure):** Pré-requisito para H2, H4 - estimativa 3-4 dias
3. **Fase 2 (Isolamento):** 8 itens (H1, H3, H5, H6, H7, H8 primeiro; H2, H4 após UID) - estimativa 8-10 dias
4. **Fase 3 (Polimento):** 12 itens - estimativa 6-8 dias
5. **Fase 5 (Low):** 2 itens - estimativa 1-2 dias

**Total estimado:** 28-38 dias de trabalho focado

---

## Notas Importantes

- Este plano assume que os itens já corrigidos (sessões anteriores) permanecem estáveis.
- Itens marcados como "REQUIRES UID INFRASTRUCTURE" devem ser implementados após Fase 4.
- Testes de regressão devem ser executados após cada fase completa.
- Alguns itens (como VMA table per process) são arquiteturais e podem ter impactos amplos.
