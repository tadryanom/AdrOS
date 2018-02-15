MAGIC_NUMBER equ 0x1BADB002     ; define o numero magico para o GRUB
FLAGS        equ 0x0            ; GRUB multiboot flags
CHECKSUM     equ -MAGIC_NUMBER  ; calcula o checksum
                                ; (numero magico + checksum + GRUB flags igual a 0)
KERNEL_STACK_SIZE equ 4096      ; tamanho da pilha em bytes

global loader                   ; declaracao global para o ponto de entrada do ELF do kernel

section .text                   ; inicio da secao de codigo
align 4                         ; alinha codigo em 4 bytes
    dd MAGIC_NUMBER             ; escreve o numero magico para o GRUB em codigo de maquina,
    dd FLAGS                    ; escreve as flags do GRUB em codigo de maquina,
    dd CHECKSUM                 ; e tambem o checksum

loader:                         ; ponto de entrada para o kernel definido no linker script)
    mov esp, kernel_stack + KERNEL_STACK_SIZE   ; aponta em no registrador esp o inicio da pilha (stack)
                                ; (stack) (fim da area de memoria)
    mov ebx, 0x000B8000         ; carrega no resgistrador ebx o endereco da memoria de video
    xor eax, eax                ; zera registro eax
    mov ah, 0xC4                ; define cores de fundo da tela e para os caracteres
                                ; aqui eu usei vermelho para o fundo e vermelho claro para os caracteres
    mov esi, wcmsg              ; carrega o endereço onde inciar a string noo registrador esi

.repeat:                        ; label para repeticao ate o fim da string
    lodsb                       ; carrega os proximos bytes da string no registrador al
                                ; avança 1 byte toda vez que essa intrucao eh executada
    or al, al                   ; compara o registrador al com 0 (zero eh usado para indicar fim da string)
    jz .loop                    ; se for igual a 0, então pula para a label .loop, se não, vai para a prox. instrucao
    mov [ebx], eax              ; coloca os dados no endereço de memoria apontado por ebx
                                ; (no caso, o endereco da memoria de video, carregado em ebx anteriormente)
    add ebx, 0x2                ; incrementa o registrador ebx (indica avanco para o proximo byte da memoria de video,
                                ; aqui, eh onde eu faco colocar o proximo caracter da string mais a direita da tela)
    jmp .repeat                 ; repete ate chegar o final da string

.loop:                          ; label para o loop eterno
    cli                         ; desativa todas a interrupcoes de hardware
    jmp .loop                   ; bom, aqui no programa permanece em um loop eterno
                                ; irei futuramente chamar a função kmain() para o kernel fazer outras coisas

section .bss                    ; inicio da secao de dados nao inicializados
align 4                         ; alinha em 4 bytes
kernel_stack:                   ; label que aponta o endereco de memoria para a pilha (stack) do kernel
    resb KERNEL_STACK_SIZE      ; reserva o endereco para a pilha (stack) do kernel

section .data                   ; inicio da secao com dados inicializados
align 4                         ; alinha em 4 bytes
wcmsg: db 'AdrOS Kernel esta sendo executado com sucesso! - criado por Tulio Muniz.',0 ; label para string
