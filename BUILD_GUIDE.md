# AdrOS - Build & Run Guide

Este guia explica como compilar e rodar o AdrOS em sua máquina local (Linux/WSL).

## 1. Dependências

Você precisará de:
- `make` e `gcc` (build system)
- `qemu-system-*` (emuladores)
- `xorriso` (para criar ISOs bootáveis)
- `grub-pc-bin` e `grub-common` (bootloader x86)
- Compiladores cruzados (Cross-compilers) para ARM/RISC-V

### No Ubuntu/Debian:
```bash
sudo apt update
sudo apt install build-essential bison flex libgmp3-dev libmpc-dev libmpfr-dev texinfo \
    qemu-system-x86 qemu-system-arm qemu-system-misc \
    grub-common grub-pc-bin xorriso mtools \
    gcc-aarch64-linux-gnu gcc-riscv64-linux-gnu
```

## 2. Compilando e Rodando (x86)

Esta é a arquitetura principal (PC padrão).

### Compilar
```bash
make ARCH=x86
```
Isso gera o arquivo `adros-x86.bin`.

### Criar ISO Bootável (GRUB)
Para x86, o repositório já inclui uma árvore `iso/` com o GRUB configurado.

```bash
make ARCH=x86 iso
```

Isso gera `adros-x86.iso`.

### Rodar no QEMU
```bash
make ARCH=x86 run
```

Saídas/artefatos gerados:
- `serial.log`: log do UART (saída principal do kernel)
- `qemu.log`: só é gerado quando o debug do QEMU está habilitado (veja abaixo)

Para habilitar debug do QEMU (mais leve por padrão, para evitar travamentos por I/O excessivo):
```bash
make ARCH=x86 run QEMU_DEBUG=1
```

## 3. Compilando e Rodando (ARM64)

### Compilar
```bash
make ARCH=arm
```
Isso gera `adros-arm.bin`.

### Rodar no QEMU
ARM não usa GRUB/ISO da mesma forma, carregamos o kernel direto na memória.

```bash
qemu-system-aarch64 -M virt -cpu cortex-a57 -m 128M -nographic \
    -kernel adros-arm.bin
```
- Para sair do QEMU sem gráfico: `Ctrl+A` solte, depois `x`.

## 4. Compilando e Rodando (RISC-V)

### Compilar
```bash
make ARCH=riscv
```
Isso gera `adros-riscv.bin`.

### Rodar no QEMU
```bash
qemu-system-riscv64 -machine virt -m 128M -nographic \
    -bios default -kernel adros-riscv.bin
```
- Para sair: `Ctrl+A`, `x`.

## 5. Troubleshooting Comum

- **"Multiboot header not found"**: Verifique se o `grub-file --is-x86-multiboot2 adros-x86.bin` retorna sucesso (0). Se falhar, a ordem das seções no `linker.ld` pode estar errada.
- **"Triple Fault (Reset infinito)"**: Geralmente erro na tabela de paginação (VMM) ou IDT mal configurada. Rode `make ARCH=x86 run QEMU_DEBUG=1` e inspecione `qemu.log`.
- **Tela preta (VGA)**: Se estiver rodando com `-nographic`, você não verá o VGA. Remova essa flag para ver a janela.
