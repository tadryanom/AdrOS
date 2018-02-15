ENTRY(loader)                /* label para o ponto de entrada do kernel*/

SECTIONS {
    . = 0x00100000;          /* aqui diz que o codigo do kernel será caregado a cima de 1 MB na memoria*/

    .text ALIGN (0x1000) :   /* alinha em 4 KB */
    {
        *(.text)             /* todos as secoes de codigo de todos os arquivos */
    }

    .rodata ALIGN (0x1000) : /* alinha em 4 KB */
    {
        *(.rodata*)          /* todas as secoes de dados somente leitura de todos os arquivos */
    }

    .data ALIGN (0x1000) :   /* alinha em 4 KB */
    {
        *(.data)             /* todas as secoes de dados de todos os arquivos */
    }

    .bss ALIGN (0x1000) :    /* alinha em 4 KB */
    {
        *(COMMON)            /* todas as secoes COMMON de todos os arquivos */
        *(.bss)              /* todas as secoes bss (dados nao inicializados) de todos os arquivos */
    }
}
