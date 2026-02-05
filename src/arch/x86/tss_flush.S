.section .text
.global tss_flush
tss_flush:
    mov 4(%esp), %ax
    ltr %ax
    ret
