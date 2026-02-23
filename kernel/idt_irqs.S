.section .text
.code64

.global nm_isr_irq0
.global nm_isr_irq1

.extern nm_irq_isr

.macro IRQ_STUB name, vec
\name:
    // Save enough registers and keep stack 16B aligned for the C call.
    pushq %r15
    pushq %r14
    pushq %r13
    pushq %r12
    pushq %r11
    pushq %r10
    pushq %r9
    pushq %r8
    pushq %rsi
    pushq %rdi
    pushq %rbp
    pushq %rdx
    pushq %rcx
    pushq %rbx
    pushq %rax

    movq $\vec, %rdi
    call nm_irq_isr

    popq %rax
    popq %rbx
    popq %rcx
    popq %rdx
    popq %rbp
    popq %rdi
    popq %rsi
    popq %r8
    popq %r9
    popq %r10
    popq %r11
    popq %r12
    popq %r13
    popq %r14
    popq %r15
    iretq
.endm

IRQ_STUB nm_isr_irq0, 32
IRQ_STUB nm_isr_irq1, 33

.section .note.GNU-stack,"",@progbits
