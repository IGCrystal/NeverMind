.section .text
.code64

.global nm_isr_ud
.global nm_isr_df
.global nm_isr_gp
.global nm_isr_pf

.extern nm_exception_dispatch

// Stack layout passed to nm_exception_dispatch (uint64_t *stack):
//   stack[0] = vector
//   stack[1] = error
//   stack[2] = RIP
//   stack[3] = CS
//   stack[4] = RFLAGS

nm_isr_ud:
    pushq $0
    pushq $6
    mov %rsp, %rdi
    andq $-16, %rsp
    call nm_exception_dispatch
1:  hlt
    jmp 1b

nm_isr_df:
    // CPU pushes an error code (0) for #DF
    pushq $8
    mov %rsp, %rdi
    andq $-16, %rsp
    call nm_exception_dispatch
1:  hlt
    jmp 1b

nm_isr_gp:
    // CPU pushes an error code for #GP
    pushq $13
    mov %rsp, %rdi
    andq $-16, %rsp
    call nm_exception_dispatch
1:  hlt
    jmp 1b

nm_isr_pf:
    // CPU pushes an error code for #PF
    pushq $14
    mov %rsp, %rdi
    andq $-16, %rsp
    call nm_exception_dispatch
1:  hlt
    jmp 1b
