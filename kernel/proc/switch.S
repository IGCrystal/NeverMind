.section .text
.global nm_context_switch
.type nm_context_switch, @function

nm_context_switch:
    // Preserve SysV ABI callee-saved registers across a task switch.
    // This function is effectively a coroutine switch point.
    pushq %rbp
    pushq %rbx
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    movq %rsp, (%rdi)
    movq %rsi, %rsp

    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbx
    popq %rbp
    ret

.size nm_context_switch, . - nm_context_switch

.section .note.GNU-stack,"",@progbits
