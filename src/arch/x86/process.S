.section .text
.global context_switch

/*
 * void context_switch(uint32_t* old_esp_ptr, uint32_t new_esp);
 */
context_switch:
    /* 1. Save state of the OLD process */
    /* Pushing registers that C calling convention expects us to preserve */
    push %ebp
    push %ebx
    push %esi
    push %edi
    pushf                /* Save EFLAGS (including IF) */
    
    /* 2. Save the old ESP */
    /* Stack is: [RetAddr] [old_esp_ptr] [new_esp] + 5 pushes (20 bytes)
       ESP points to EFLAGS (last push) */
       
    mov 24(%esp), %eax   /* EAX = old_esp_ptr */
    mov %esp, (%eax)     /* *old_esp_ptr = ESP */
    
    /* 3. Switch to the NEW process stack */
    mov 28(%esp), %esp   /* ESP = new_esp */
    
    /* 4. Restore state of the NEW process */
    popf                 /* Restore EFLAGS (including IF) from saved context */
    pop %edi
    pop %esi
    pop %ebx
    pop %ebp
    
    /* 5. Return */
    /* Since we changed ESP, this 'ret' pops the EIP from the NEW stack! */
    ret

 .section .note.GNU-stack,"",@progbits
