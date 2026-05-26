# VFS Umount Root Check - TODO

## Status
- **Item 11**: VFS Umount Refcount Checks
- **Check de cwd**: ✅ Implementado e validado
- **Check de root**: ❌ Pendente

## Implementado ✅

### Check de cwd
Implementado via mount refcounting por path:
- Funções: `vfs_mount_ref_by_path()`, `vfs_mount_unref_by_path()`
- chdir atualiza refcounts (unref old cwd, ref new cwd)
- Process creation/exit gerenciam refcounts de cwd
- vfs_umount_nolock verifica refcount > 0
- Teste I20 (umount cwd) passando
- **Resultado**: 124/124 testes passando

## Não Implementado ❌

### Check de root
Pendente devido a complexidade e riscos de estabilidade.

### Motivos para não implementar

#### Tentativa 1: char root[128] em struct process
- **Problema**: Causou corrupção de memória e instabilidade massiva
- **Causa**: Problemas de alinhamento na estrutura process
- **Resultado**: Testes passaram de 119/119 para 77/119 (42 falhas)
- **Ação**: Revertido

#### Tentativa 2: fs_node_t* root_fs_node em struct process
- **Problema**: Causou instabilidade massiva (55/125 testes passando)
- **Causa**: Modificação de vfs_lookup_depth para usar root_fs_node causou problemas:
  - current_process pode ser NULL em contextos de kernel
  - Acesso a root_fs_node sem locks adequados
  - Problemas de refcount quando processos saem
- **Ação**: Revertido

## Opções Futuras para Implementação

### Opção 1: Per-process mount namespaces (mais robusto)
- **Complexidade**: Muito Alta (5-7 dias)
- **Descrição**: Implementar mount namespaces per-process
  - Criar struct mount_namespace com tabela de mounts per-process
  - Copy-on-write de mount namespaces (clone com CLONE_NEWNS)
  - Gerenciamento de refcount de namespaces
  - Modificar pivot_root para operar no namespace do processo atual
  - Atualizar todas funções VFS para usar namespace do processo
- **Vantagens**: Implementação completa e correta, base para containers
- **Riscos**: Muito alto risco de regressões, complexidade extrema

### Opção 2: Resolver problemas de alinhamento
- **Complexidade**: Média (2-3 dias)
- **Descrição**: Investigar e resolver problemas de alinhamento ao adicionar campo root
  - Mover campo para final da struct (antes de fpu_state)
  - Testar cuidadosamente com todos os processos (kernel, idle, user)
  - Usar path string em vez de fs_node_t*
- **Vantagens**: Mais simples que namespaces
- **Riscos**: Pode não resolver completamente o problema

### Opção 3: Investigar fs_node_t* approach
- **Complexidade**: Alta (3-4 dias)
- **Descrição**: Debugar por que fs_node_t* causou instabilidade
  - Adicionar checks NULL para current_process em vfs_lookup_depth
  - Adicionar locks adequados para acesso a root_fs_node
  - Verificar refcount management em process exit
- **Vantagens**: Usa infraestrutura de refcount existente
- **Riscos**: Pode não ser possível resolver sem arquitetura de namespaces

## Recomendação Atual
Manter apenas check de cwd (já funcional) e considerar check de root como feature futura quando houver necessidade real (uso de chroot/pivot_root em aplicações).

## Testes Atuais
- Smoke test: 124/124 PASS
- Zero regressões após reverter chroot

## Quando Implementar
Implementar check de root quando:
1. Houver necessidade real de usar chroot/pivot_root em aplicações
2. Ou quando implementar per-process mount namespaces para suporte a containers
3. Ou quando houver tempo/budget para investigar a causa da instabilidade

## Referências
- Item 11: VFS Umount Refcount Checks
- Documento de auditoria: docs/FULL_POSIX_AUDIT.md
- Roadmap POSIX: docs/POSIX_ROADMAP.md
