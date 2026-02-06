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
    
    /* 2. Save the old ESP */
    /* Get the location of old_esp_ptr. 
       Stack is: [RetAddr] [old_esp_ptr] [new_esp] 
       ESP points to EDI (last push) */
       
    mov 20(%esp), %eax   /* EAX = old_esp_ptr */
    mov %esp, (%eax)     /* *old_esp_ptr = ESP */
    
    /* 3. Switch to the NEW process stack */
    mov 24(%esp), %esp   /* ESP = new_esp */
    
    /* 4. Restore state of the NEW process */
    pop %edi
    pop %esi
    pop %ebx
    pop %ebp
    
    /* 5. Return */
    /* Since we changed ESP, this 'ret' pops the EIP from the NEW stack! */
    ret

 .section .note.GNU-stack,"",@progbits
