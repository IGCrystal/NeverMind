.section .text
.code64

.global nm_kthread_trampoline

// Stack layout on first switch-in:
//   ret -> nm_kthread_trampoline
//   [rsp]     = entry (uint64_t)
//   [rsp + 8] = arg   (uint64_t)

nm_kthread_trampoline:
    popq %rax        // entry
    popq %rdi        // arg
    call *%rax
1:  hlt
    jmp 1b

.section .note.GNU-stack,"",@progbits
