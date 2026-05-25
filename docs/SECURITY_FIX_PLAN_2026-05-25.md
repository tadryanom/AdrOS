# Plano de Correção de Segurança - AdrOS 2026-05-25

## Resumo Executivo

Este documento estabelece um plano priorizado para corrigir as vulnerabilidades identificadas no relatório de reauditoria de segurança de 2026-05-25. O plano está organizado em 5 fases, priorizando isolamento de memória, bypasses de filesystem, parsers de boot/storage, interfaces multiusuário e correções userspace/libc.

**Status atual:** 36 problemas identificados, 9 já corrigidos em sessões anteriores, 27 pendentes.

**Correções confirmadas já feitas:**
- K01: mmap(MAP_FIXED) validação de alinhamento/overflow/kernel crossing
- K03: shm_at validação de endereço e busca de área livre
- K04: AIO com bounce buffers
- K05: send/recv/sendto/recvfrom com bounce buffers
- A06: kill checa root ou UID/EUID compatível
- K08: clone rejeita flags não suportadas, refcount de address space
- A17: vdso.tick_hz inicializado com TIMER_HZ
- K11: diskfs antigo removido, mount usa tipo explícito
- K07: O_EXCL e O_DIRECTORY implementados
- A16: pmm_free_page ganhou guarda contra double free

---

## Fase 1: Isolamento de Memória (Crítico)

### 1.1 pmm_decref underflow (A16) - ALTA PRIORIDADE

**Problema:** `pmm_decref` decrementa `frame_refcount[frame]` sem checar se o valor já é zero, permitindo underflow.

**Arquivo:** `src/mm/pmm.c:243-254`

**Código atual:**
```c
uint32_t pmm_decref(uintptr_t paddr) {
    uint64_t frame = paddr / PAGE_SIZE;
    if (frame == 0 || frame >= max_frames) return 0;
    uintptr_t flags = spin_lock_irqsave(&pmm_lock);
    uint32_t new_val = --frame_refcount[frame];  // <-- underflow possível
    if (new_val == 0) {
        bitmap_unset(frame);
        used_memory -= PAGE_SIZE;
    }
    spin_unlock_irqrestore(&pmm_lock, flags);
    return new_val;
}
```

**Correção:**
```c
uint32_t pmm_decref(uintptr_t paddr) {
    uint64_t frame = paddr / PAGE_SIZE;
    if (frame == 0 || frame >= max_frames) return 0;
    uintptr_t flags = spin_lock_irqsave(&pmm_lock);
    if (frame_refcount[frame] == 0) {
        // Underflow - bug de ownership, reportar e abortar
        spin_unlock_irqrestore(&pmm_lock, flags);
        kprintf("[PMM] ERROR: pmm_decref underflow on frame %llu\n", frame);
        return 0;
    }
    uint32_t new_val = --frame_refcount[frame];
    if (new_val == 0) {
        bitmap_unset(frame);
        used_memory -= PAGE_SIZE;
    }
    spin_unlock_irqrestore(&pmm_lock, flags);
    return new_val;
}
```

**Teste:** Adicionar teste em fulltest que tenta decref excessivo.

---

### 1.2 sysenter.S validação insuficiente (A02) - ALTA PRIORIDADE

**Problema:** sysenter valida apenas `ECX < 0xC0000000`, mas não checa:
- ECX não nulo
- ECX + 8 ainda em userspace
- Memória mapeada e legível
- Leitura não cruza página inválida

**Arquivo:** `src/arch/x86/sysenter.S:87-95`

**Código atual:**
```asm
cmpl $0xC0000000, %ecx
jae  1f
mov 4(%ecx), %edx   /* EDX = arg3 (was saved by user) */
mov 8(%ecx), %ecx   /* ECX = arg2 (was saved by user) */
jmp 2f
1:  /* Invalid user ESP — zero args to prevent data leak */
xor %edx, %edx
xor %ecx, %ecx
```

**Correção:** Usar rotina segura de copy_from_user ou validar mais robustamente:
```asm
cmpl $0xC0000000, %ecx
jae  1f
cmpl $8, %ecx        /* ECX must be at least 8 bytes from kernel boundary */
jb   1f
/* Adicionar check de página mapeada via page table walk seria ideal,
   mas complexo em assembly. Alternativa: usar copy_from_user em C */
mov 4(%ecx), %edx
mov 8(%ecx), %ecx
jmp 2f
1:
xor %edx, %edx
xor %ecx, %ecx
```

**Nota:** Solução ideal seria mover a recuperação de args para C usando `copy_from_user`, mas isso requer mudança de ABI. Por enquanto, adicionar check de limite mínimo.

---

### 1.3 W^X/NX default em mmap/brk/SHM (A01) - ALTA PRIORIDADE

**Problema:** VMM_FLAG_NX existe mas não é aplicado por default em:
- syscall_mmap_impl: memória anônima mapeada sem NX
- brk: heap mapeado sem NX
- shm_at: SHM mapeado sem NX

**Arquivos:** `src/kernel/syscall.c`, `src/arch/x86/vmm.c`, `src/kernel/shm.c`

**Política proposta:** Todo mapeamento user sem PROT_EXEC explícito deve ter NX.

**Correção em syscall_mmap_impl:**
```c
/* Ao calcular flags de PTE */
uint32_t prot_flags = 0;
if (prot & PROT_READ)  prot_flags |= VMM_FLAG_PRESENT | VMM_FLAG_USER;
if (prot & PROT_WRITE) prot_flags |= VMM_FLAG_WRITE;
if (prot & PROT_EXEC)  prot_flags |= VMM_FLAG_USER;
else                    prot_flags |= VMM_FLAG_NX;  /* NX por default */
```

**Correção em brk:**
```c
/* brk sempre mapeia heap como RW, nunca executável */
uint32_t flags = VMM_FLAG_PRESENT | VMM_FLAG_WRITE | VMM_FLAG_USER | VMM_FLAG_NX;
```

**Correção em shm_at:**
```c
/* SHM por default RW sem execução */
uint32_t flags = VMM_FLAG_PRESENT | VMM_FLAG_WRITE | VMM_FLAG_USER | VMM_FLAG_NX;
```

---

### 1.4 mprotect por VMA (K02) - ALTA PRIORIDADE

**Problema:** mprotect valida kernel crossing e aplica NX, mas tem fallback permissivo para endereços user acima de 0x0800000, permitindo alterar permissões de regiões não rastreadas (heap, stack, texto).

**Arquivo:** `src/kernel/syscall.c`

**Solução arquitetural:** Implementar tabela unificada de VMAs por processo cobrindo:
- Heap
- Stack
- mmap regions
- Loader segments
- SHM

**Plano de implementação:**
1. Criar `struct vma { start, end, prot, flags }` em `include/vmm.h`
2. Adicionar `vma_list` em `struct process`
3. mprotect só opera em ranges completamente cobertos por VMAs do processo
4. Adicionar helpers `vma_find`, `vma_add`, `vma_remove`
5. Atualizar mmap, brk, loader para registrar VMAs

**Nota:** Esta é uma mudança arquitetural significativa. Pode ser implementada em fases:
- Fase 1a: Adicionar VMA tracking para mmap apenas
- Fase 1b: Estender para heap/stack/loader
- Fase 1c: Integrar com mprotect

---

## Fase 2: Bypasses de Filesystem (Crítico)

### 2.1 pread/pwrite ignoram modo do fd (A03) - ALTA PRIORIDADE

**Problema:** read/write agora validam O_WRONLY/O_RDONLY, mas pread/pwrite chamam diretamente `node->f_ops->read/write` sem validar modo nem mount read-only.

**Arquivo:** `src/kernel/syscall.c:4335-4380`

**Código atual (pread):**
```c
if (syscall_no == SYSCALL_PREAD) {
    int fd = (int)sc_arg0(regs);
    void* buf = (void*)sc_arg1(regs);
    uint32_t count = sc_arg2(regs);
    uint32_t offset = sc_arg3(regs);
    struct file* f = fd_get(fd);
    if (!f || !f->node) { sc_ret(regs) = (uint32_t)-EBADF; return; }
    if (!(f->node->f_ops && f->node->f_ops->read)) { sc_ret(regs) = (uint32_t)-ESPIPE; return; }
    if (count > 1024 * 1024) { sc_ret(regs) = (uint32_t)-EINVAL; return; }
    /* ... chama f->node->f_ops->read diretamente ... */
}
```

**Correção:** Adicionar validação de modo similar a read/write:
```c
if (syscall_no == SYSCALL_PREAD) {
    int fd = (int)sc_arg0(regs);
    void* buf = (void*)sc_arg1(regs);
    uint32_t count = sc_arg2(regs);
    uint32_t offset = sc_arg3(regs);
    struct file* f = fd_get(fd);
    if (!f || !f->node) { sc_ret(regs) = (uint32_t)-EBADF; return; }
    if (!(f->node->f_ops && f->node->f_ops->read)) { sc_ret(regs) = (uint32_t)-ESPIPE; return; }
    
    /* Validar modo do fd */
    if ((f->flags & 3U) == O_WRONLY) {
        sc_ret(regs) = (uint32_t)-EBADF;  /* Cannot read from write-only */
        return;
    }
    
    /* Validar mount read-only */
    int rc = vfs_require_writable_path(NULL);  /* Precisa de path, ver abaixo */
    if (rc < 0) { sc_ret(regs) = (uint32_t)rc; return; }
    
    if (count > 1024 * 1024) { sc_ret(regs) = (uint32_t)-EINVAL; return; }
    /* ... */
}
```

**Para pwrite:** Adicionar check O_RDONLY e vfs_require_writable_path.

**Nota:** vfs_require_writable_path precisa de path. pread/pwrite não têm path. Solução:
- Adicionar campo `mount_root` em `struct file` (já existe da fase VFS)
- Usar `vfs_node_mount_flags(f->mount_root)` para checar MS_RDONLY

---

### 2.2 O_RDONLY|O_TRUNC pode truncar cedo demais (A04) - ALTA PRIORIDADE

**Problema:** O caminho de open executa truncamento quando O_TRUNC está presente. A checagem usa `(flags & 3U) != 0`, mas O_RDONLY costuma ser zero. Assim, O_RDONLY|O_TRUNC pode passar.

**Arquivo:** `src/kernel/syscall.c:2400-2406`

**Código atual:**
```c
} else if ((flags & 0x200U) != 0U && node->flags == FS_FILE) {
    /* O_TRUNC on existing file */
    int rc = vfs_require_writable_path(path);
    if (rc < 0) return rc;
    if (node->i_ops && node->i_ops->truncate) {
        node->i_ops->truncate(node, 0);
        node->length = 0;
```

**Correção:** Exigir explicitamente O_WRONLY ou O_RDWR para O_TRUNC:
```c
} else if ((flags & 0x200U) != 0U && node->flags == FS_FILE) {
    /* O_TRUNC on existing file - require write access */
    if ((flags & 3U) == O_RDONLY) {
        return -EACCES;  /* Cannot truncate with O_RDONLY */
    }
    int rc = vfs_require_writable_path(path);
    if (rc < 0) return rc;
    if (node->i_ops && node->i_ops->truncate) {
        node->i_ops->truncate(node, 0);
        node->length = 0;
```

---

### 2.3 Permissões VFS incompletas (A07, K06, K20) - ALTA PRIORIDADE

**Problema:** 
- execve verifica MS_NOEXEC mas não exige bit executável no inode
- Operações de diretório pai (unlink, rmdir, rename, link, mkdir, criação) não aplicam checagem completa de escrita/execução no pai
- vfs_check_permission trata mode==0 como permissivo
- Não há suporte a grupos suplementares/capabilities

**Arquivos:** `src/kernel/fs.c`, `src/kernel/syscall.c`

**Correções necessárias:**

1. **execve:** Adicionar check de bit executável:
```c
/* Em syscall_execve_impl, após lookup */
if (!(node->mode & 0111)) {  /* Bit executável */
    return -EACCES;
}
```

2. **Operações de diretório pai:** Criar helper `vfs_check_parent_permission(path, perm)`:
```c
int vfs_check_parent_permission(const char* path, int perm) {
    /* Extrair diretório pai */
    char parent[256];
    extract_parent_path(path, parent);
    
    /* Lookup do pai */
    fs_node_t* parent_node = vfs_lookup(parent);
    if (!parent_node) return -ENOENT;
    
    /* Checar permissão no pai */
    return vfs_check_permission(parent_node, perm);
}
```

Aplicar em:
- unlink: checar w+x no pai
- rmdir: checar w+x no pai
- rename: checar w+x em ambos os pais
- link: checar w+x no pai do novo link
- mkdir: checar w+x no pai
- create: checar w+x no pai

3. **vfs_check_permission:** Remover fallback permissivo para mode==0:
```c
/* Se mode==0, tratar como 0000 (sem permissões) em vez de permissivo */
if (mode == 0) {
    /* Ainda checar ownership */
    if (uid == current_process->uid) return 0;
    return -EACCES;
}
```

---

### 2.4 access() ignora modo solicitado (A19) - MÉDIA PRIORIDADE

**Problema:** SYSCALL_ACCESS copia o caminho, chama lookup e retorna sucesso se o caminho existir. O argumento mode não é aplicado.

**Arquivo:** `src/kernel/syscall.c`

**Correção:** Implementar F_OK, R_OK, W_OK, X_OK:
```c
if (syscall_no == SYSCALL_ACCESS) {
    const char* path = (const char*)sc_arg0(regs);
    int mode = (int)sc_arg1(regs);
    
    char kpath[256];
    int rc = copy_user_cstr(path, kpath, sizeof(kpath));
    if (rc < 0) { sc_ret(regs) = (uint32_t)rc; return; }
    
    fs_node_t* node = vfs_lookup(kpath);
    if (!node) { sc_ret(regs) = (uint32_t)-ENOENT; return; }
    
    /* F_OK: apenas existência */
    if (mode == F_OK) {
        sc_ret(regs) = 0;
        return;
    }
    
    /* R_OK/W_OK/X_OK: checar permissões reais */
    int perm = 0;
    if (mode & R_OK) perm |= 0444;
    if (mode & W_OK) perm |= 0222;
    if (mode & X_OK) perm |= 0111;
    
    rc = vfs_check_permission(node, perm);
    sc_ret(regs) = (uint32_t)rc;
    return;
}
```

---

### 2.5 O_NOFOLLOW ignorado (K21) - MÉDIA PRIORIDADE

**Problema:** O_EXCL e O_DIRECTORY foram implementados, mas O_NOFOLLOW ainda não é efetivo. vfs_lookup_depth segue symlink automaticamente.

**Arquivos:** `src/kernel/syscall.c`, `src/kernel/fs.c`

**Correção:**
1. Adicionar flag de lookup em `vfs_lookup_depth`:
```c
/* Adicionar parâmetro int flags */
fs_node_t* vfs_lookup_depth(const char* path, int flags);
#define LOOKUP_FOLLOW 0x01
#define LOOKUP_NOFOLLOW 0x02
```

2. Em syscall_open_impl, propagar O_NOFOLLOW:
```c
int lookup_flags = LOOKUP_FOLLOW;
if (flags & O_NOFOLLOW) {
    lookup_flags = LOOKUP_NOFOLLOW;
}
fs_node_t* node = vfs_lookup_depth(kpath, lookup_flags);
```

3. Em vfs_lookup_depth, recusar symlink no componente final quando NOFOLLOW:
```c
/* Se último componente é symlink e NOFOLLOW está setado */
if ((flags & LOOKUP_NOFOLLOW) && (last_node->flags & FS_SYMLINK)) {
    return -ELOOP;  /* ou -EMLINK */
}
```

---

### 2.6 truncate/ftruncate não atualizam storage real (A14) - ALTA PRIORIDADE

**Problema:** open(O_TRUNC) chama i_ops->truncate quando disponível, mas TRUNCATE/FTRUNCATE syscalls alteram diretamente node->length sem chamar o backend.

**Arquivo:** `src/kernel/syscall.c`

**Correção:** Rotear todas as formas de truncamento por vfs_truncate/i_ops->truncate:
```c
/* Criar vfs_truncate que chama i_ops->truncate quando disponível */
int vfs_truncate(fs_node_t* node, off_t length) {
    if (node->i_ops && node->i_ops->truncate) {
        return node->i_ops->truncate(node, length);
    }
    /* Fallback para FS que não suportam truncamento real */
    node->length = length;
    return 0;
}

/* Em SYSCALL_TRUNCATE e SYSCALL_FTRUNCATE, usar vfs_truncate */
```

---

## Fase 3: Parsers de Boot/Storage (Crítico)

### 3.1 initrd/LZ4/TAR parsers inseguros (A05) - ALTA PRIORIDADE

**Problema:** Parser acessa bytes do buffer antes de validar tamanho mínimo. Leitura de magic sem garantir size >= 4, caminho LZ4 consulta header sem validar tamanho, loop TAR itera sem limite forte.

**Arquivo:** `src/drivers/initrd.c`

**Correção:** Transformar parser em máquina de estados limitada por (base, size):
```c
/* Adicionar validação de tamanho mínimo antes de cada acesso */
if (size < 4) return -EINVAL;
uint32_t magic = *(uint32_t*)raw;
if (magic != EXPECTED_MAGIC) return -EINVAL;

/* Para TAR: validar rec_len, name_len, overflow antes de avançar */
while (location < size) {
    if (location + 512 > size) break;  /* Limite de bloco */
    
    /* Validar rec_len */
    uint32_t rec_len = parse_octal(header + 124, 12);
    if (rec_len < 8 || rec_len > 4096) return -EINVAL;  /* rec_len deve ser >= 8 */
    if (location + rec_len > size) return -EINVAL;  /* Overflow */
    
    /* Validar name_len */
    uint32_t name_len = strnlen(header, 100);
    if (name_len >= rec_len - 8) return -EINVAL;  /* name_len deve caber em rec_len - 8 */
    
    location += rec_len;
}
```

---

### 3.2 Multiboot2 parsers sem validação de limites (A15) - ALTA PRIORIDADE

**Problema:** Código copia estrutura Multiboot2 mas itera tags até END sem checar que cada tag permanece dentro de total_size/buffer copiado e sem validar tamanho mínimo/alinhamento.

**Arquivos:** `src/arch/x86/arch_early_setup.c`, `src/arch/x86/pmm_boot.c`

**Correção:**
```c
/* Validar total_size */
if (total_size > buffer_size) return -EINVAL;

uint32_t cursor = 8;  /* Skip header */
while (cursor + 8 <= base + total_size) {
    multiboot_tag_t* tag = (multiboot_tag_t*)(base + cursor);
    
    /* Validar tamanho mínimo e alinhamento */
    if (tag->size < 8) return -EINVAL;
    if (tag->size % 8 != 0) return -EINVAL;  /* Deve ser alinhado */
    
    /* Validar limite */
    if (cursor + tag->size > base + total_size) return -EINVAL;
    
    if (tag->type == MULTIBOOT_TAG_TYPE_END) break;
    
    cursor += tag->size;
}
```

---

### 3.3 ext2 parsing vulnerável a mídia malformada (F01) - ALTA PRIORIDADE

**Problema:** Loops de diretório aceitam entradas sem validar completamente: rec_len deve ser >= 8, alinhado, caber no bloco; name_len deve caber em rec_len - 8; superbloco/GDT precisam de validação de overflow.

**Arquivo:** `src/kernel/ext2.c`

**Correção:** Adicionar validadores estritos:
```c
/* Em ext2_readdir */
while (offset < block_size) {
    ext2_dirent_t* dirent = (ext2_dirent_t*)(block + offset);
    
    /* Validar rec_len */
    if (dirent->rec_len < 8) return -EIO;
    if (dirent->rec_len % 4 != 0) return -EIO;  /* Deve ser alinhado */
    if (offset + dirent->rec_len > block_size) return -EIO;
    
    /* Validar name_len */
    if (dirent->name_len >= dirent->rec_len - 8) return -EIO;
    
    offset += dirent->rec_len;
}

/* Validar superbloco */
if (superblock->magic != EXT2_MAGIC) return -EINVAL;
if (superblock->inode_count == 0) return -EINVAL;
if (superblock->block_count == 0) return -EINVAL;
/* Validar overflow de grupos */
if (superblock->blocks_per_group == 0) return -EINVAL;
uint32_t group_count = (superblock->block_count + superblock->blocks_per_group - 1) / superblock->blocks_per_group;
if (group_count > 1024) return -EINVAL;  /* Limite razoável */
```

---

## Fase 4: Interfaces Multiusuário (Alta Prioridade)

### 4.1 /proc vaza informações e não segura lifetime (K12, K13, K23) - ALTA PRIORIDADE

**Problema:** /proc/dmesg, /proc/cmdline, /proc/<pid>/status, /proc/<pid>/maps, /proc/<pid>/cmdline não aplicam controle de acesso por UID/EUID. Leituras por PID buscam process_t sem pin/refcount.

**Arquivo:** `src/kernel/procfs.c`

**Correção:**
1. **Política hidepid/ptrace-like:** Exigir mesmo UID ou root:
```c
/* Adicionar helper */
int proc_check_access(struct process* target) {
    if (!current_process) return -EACCES;
    if (current_process->uid == 0) return 0;  /* root */
    if (current_process->uid == target->uid) return 0;
    return -EACCES;
}
```

2. **Pin/refcount durante leitura:** Usar `process_ref`/`process_unref`:
```c
/* Em proc_pid_status_read */
struct process* p = process_find(pid);
if (!p) return -ENOENT;
if (proc_check_access(p) < 0) return -EACCES;
process_ref(p);  /* Pin */
/* ... ler dados ... */
process_unref(p);  /* Unpin */
```

3. **Redigir endereços em /proc/<pid>/maps:** Não implementar maps ou redigir endereços como zeros.

---

### 4.2 SHM sem modelo de permissão e RW sem NX (K14, K24) - ALTA PRIORIDADE

**Problema:** Segmentos SHM não carregam uid/gid/mode nem checagem de permissão. Mapeamento usa flags RW user sem NX.

**Arquivo:** `src/kernel/shm.c`

**Correção:**
1. **Adicionar metadados de permissão:**
```c
struct shm_segment {
    /* ... campos existentes ... */
    uid_t uid;
    gid_t gid;
    mode_t mode;
};
```

2. **Checagem de permissão em shm_at:**
```c
/* Em shm_at */
if (shm->uid != current_process->uid && current_process->uid != 0) {
    return -EACCES;
}
```

3. **NX por default em shm_at:**
```c
uint32_t flags = VMM_FLAG_PRESENT | VMM_FLAG_WRITE | VMM_FLAG_USER | VMM_FLAG_NX;
```

4. **Suporte a attach read-only:** Adicionar flag SHM_RDONLY em shm_at.

---

### 4.3 Raw sockets sem privilégio (K15) - ALTA PRIORIDADE

**Problema:** ksocket_create aceita SOCK_RAW e cria PCB raw no lwIP sem checar root/capacidade.

**Arquivos:** `src/kernel/socket.c`, `src/kernel/syscall.c`

**Correção:** Exigir root para SOCK_RAW:
```c
/* Em ksocket_create */
if (type == SOCK_RAW && current_process->uid != 0) {
    return -EPERM;
}
```

---

### 4.4 Futex chaveado somente por endereço virtual (K17) - ALTA PRIORIDADE

**Problema:** Tabela de futex armazena addr e processo, wake compara endereço virtual. Não há chave por address space, permitindo interferência entre processos com mesmo VA.

**Arquivo:** `src/kernel/syscall.c`

**Correção:** Chavear futex por (mm, uaddr):
```c
struct futex_waiter {
    struct address_space* mm;  /* Adicionar address space */
    void* uaddr;
    struct process* process;
    /* ... */
};

/* Em FUTEX_WAIT, usar (mm, uaddr) como chave */
/* Em FUTEX_WAKE, match por (mm, uaddr) */
```

Adicionar lock robusto para lista global de waiters.

---

### 4.5 dlopen global por processo (K22) - ALTA PRIORIDADE

**Problema:** dl_table[DLOPEN_MAX_LIBS] é global no kernel. Handles e símbolos não são isolados por processo.

**Arquivo:** `src/kernel/syscall.c`

**Correção:** Mover tabela para process_t/address space:
```c
/* Em struct process ou struct address_space */
struct dl_handle {
    char name[64];
    void* base;
    uint32_t size;
    /* ... */
};
struct dl_handle dl_handles[DLOPEN_MAX_LIBS];
int dl_handle_count;

/* Em dlopen, usar tabela do processo atual */
/* Em exit, liberar todos os handles do processo */
```

---

## Fase 5: Correções Userspace/Lib (Média Prioridade)

### 5.1 Wrappers variadicas de libc com UB (A17) - MÉDIA PRIORIDADE

**Problema:** open, openat e fcntl leem argumentos opcionais de variadic mesmo quando a chamada não exige esse argumento.

**Arquivos:** `user/ulibc/src/unistd.c`, `newlib/libgloss/adros/posix_stubs.c`

**Correção:**
```c
/* Em open */
int open(const char* pathname, int flags, ...) {
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, mode_t);
        va_end(ap);
    }
    return _syscall3(SYS_OPEN, pathname, flags, mode);
}
```

Aplicar padrão similar para openat e fcntl.

---

### 5.2 execl/execlp varargs incorreto (U04) - MÉDIA PRIORIDADE

**Problema:** Funções percorrem argumentos variadicos por aritimética sobre &arg, em vez de usar va_list.

**Arquivo:** `user/ulibc/src/execvp.c`

**Correção:** Reimplementar com va_start, va_arg:
```c
int execl(const char* path, const char* arg0, ...) {
    va_list ap;
    va_start(ap, arg0);
    
    /* Contar argumentos */
    int argc = 1;
    const char* arg;
    while ((arg = va_arg(ap, const char*)) != NULL) {
        argc++;
    }
    va_end(ap);
    
    /* Construir argv */
    char* argv[64];
    va_start(ap, arg0);
    argv[0] = (char*)arg0;
    for (int i = 1; i < argc; i++) {
        argv[i] = va_arg(ap, char*);
    }
    argv[argc] = NULL;
    va_end(ap);
    
    return execve(path, argv, environ);
}
```

---

### 5.3 scanf %s sem limite (U02) - MÉDIA PRIORIDADE

**Problema:** Implementações de scanf copiam strings sem exigir largura de campo nem conhecer tamanho do destino.

**Arquivo:** `user/ulibc/src/stdio.c`

**Correção:** Respeitar larguras de campo ou fornecer variantes seguras:
```c
/* Se %s sem largura, usar limite padrão (ex: 255) */
if (*fmt == 's' && width == 0) {
    width = 255;
}
```

Idealmente fornecer snprintf_s ou similar.

---

### 5.4 Shell substituição de comando malformada (A18) - BAIXA PRIORIDADE

**Problema:** expand_vars monta comando adicionando '(' antes do texto, mas não adiciona ')' no final.

**Arquivo:** `user/cmds/sh/sh.c`

**Correção:** Corrigir montagem da string:
```c
/* Adicionar ')' no final */
cmd[cmdlen] = ')';
cmd[cmdlen + 1] = '\0';
```

Ou implementar parser próprio para substituição sem reescrever para forma parcial.

---

### 5.5 mkstemp/tmpfile/tmpnam previsíveis (U01) - MÉDIA PRIORIDADE

**Problema:** mkstemp gera nomes com PID/counter previsíveis. tmpfile/tmpnam seguem inseguros.

**Arquivos:** `user/ulibc/src/stdlib.c`, `user/ulibc/src/stdio.c`

**Correção:**
1. Alimentar mkstemp com CSPRNG quando disponível
2. Sempre usar O_CREAT|O_EXCL
3. Passar modo explicitamente
4. tmpfile criar+unlink de forma atômica

---

### 5.6 posix_spawn quebrado (A13) - MÉDIA PRIORIDADE

**Problema:** Kernel chama fork_impl e testa child_pid == 0 no mesmo fluxo. Wrapper userspace sobrescreve PID com zero.

**Arquivos:** `src/kernel/syscall.c`, `user/ulibc/src/spawn.c`

**Correção:** Implementar posix_spawn como syscall atômica no kernel, ou fazer wrapper userspace baseado em fork + execve + _exit preservando PID corretamente.

---

## Itens Deferidos (Arquiteturais/Fora de Escopo)

### A08: Overlayfs semântica incompleta
**Razão:** Requer redesign completo de coherência e lifetime. Fora de escopo de hardening imediato.

### A09: Tmpfs sem limites/locking
**Razão:** Requer arquitetura de quotas e locks por filesystem. Pode ser endereçado em fase separada.

### A10: PRNGs determinísticos
**Razão:** Requer fonte de entropia de hardware. CSPRNG é projeto separado.

### A11: Rollback com array fixo
**Razão:** Limitação aceitável para hobby OS. Solução ideal requer rollback por caminhada de range.

### A12: VA fixa e colisão virtio_blk
**Razão:** Requer alocador de VA kernel para mapeamentos temporários. Projeto arquitetural significativo.

### K16: Truncamento silencioso de caminhos
**Razão:** Alguns caminhos já retornam ENAMETOOLONG. Completar consistência é trabalho de polimento.

### K34: sendmsg/recvmsg passam iov de usuário
**Razão:** Requer refatoração significativa de camada de socket. Pode ser endereçado em fase de network.

### K35: copy_to_user falhas ignoradas
**Razão:** Correção simples mas requer revisão de todos os caminhos de socket.

### K36: rumpuser_malloc alinhado
**Razão:** Código rump é isolado e pouco usado. Correção de baixo impacto.

### F02: fb0_mmap ignora prot
**Razão:** Framebuffer é dispositivo especial. Baixo risco em single-user OS.

### U03: getlogin/who fixos
**Razão:** Aceitável para single-user OS sem banco de usuários/sessões.

---

## Ordem de Implementação Sugerida

### Round 1 (Críticos de memória - 1-2 dias)
1. pmm_decref underflow (A16)
2. sysenter.S validação (A02)
3. W^X/NX default em mmap/brk/SHM (A01)

### Round 2 (Bypasses filesystem - 2-3 dias)
4. pread/pwrite validação modo (A03)
5. O_RDONLY|O_TRUNC (A04)
6. truncate/ftruncate atualizam storage (A14)
7. Permissões VFS básicas (execve bit X, operações pai) (A07)

### Round 3 (Parsers - 1-2 dias)
8. initrd/LZ4/TAR validação (A05)
9. Multiboot2 validação (A15)
10. ext2 validação estrita (F01)

### Round 4 (Multiuser - 2-3 dias)
11. /proc acesso/leak (K12, K13, K23)
12. SHM permissões/NX (K14, K24)
13. raw sockets privilégio (K15)
14. futex chave por (mm, uaddr) (K17)
15. dlopen global por processo (K22)

### Round 5 (Userspace - 1-2 dias)
16. varargs open/openat/fcntl (A17)
17. execl/execlp varargs (U04)
18. scanf %s limite (U02)
19. mkstemp/tmpfile/tmpnam (U01)
20. posix_spawn (A13)

### Round 6 (Polimento - 1 dia)
21. access() implementa modo (A19)
22. O_NOFOLLOW (K21)
23. shell substituição comando (A18)
24. copy_to_user falhas em socket (K35)

**Total estimado:** 10-13 dias de trabalho focado.

---

## Critérios de Sucesso

- Todos os testes existentes continuam passando (smoke, battery, host)
- Zero regressões em funcionalidade
- cppcheck limpo nos arquivos modificados
- Cada correção tem teste correspondente em fulltest quando aplicável
- Documentação atualizada com mudanças arquiteturais (VMA, SHM permissões, etc.)

---

## Referências

- Relatório de auditoria original: docs/AUDIT_REPORT.md
- Histórico de correções: commits 91377cf, c3476f0, 3aa5178, 36476bc, 1f46f9e, bc8a27b
- Plano VFS/Mount: docs/POSIX_ROADMAP.md
