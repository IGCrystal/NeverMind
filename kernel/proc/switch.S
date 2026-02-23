.section .text
.global nm_context_switch
.type nm_context_switch, @function

nm_context_switch:
    movq %rsp, (%rdi)
    movq %rsi, %rsp
    ret

.size nm_context_switch, . - nm_context_switch

.section .note.GNU-stack,"",@progbits
