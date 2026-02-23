.set KERNEL_VIRT_BASE, 0xFFFFFFFF80000000
.set MB2_MAGIC, 0xE85250D6
.set MB2_ARCH_I386, 0

.set CR0_PE, 0x1
.set CR0_PG, 0x80000000
.set CR4_PAE, 0x20

.set EFER_MSR, 0xC0000080
.set EFER_LME, 0x100

.set PTE_P, 0x1
.set PTE_RW, 0x2
.set PTE_PS, 0x80

.section .multiboot2,"a"
.align 8
multiboot2_header:
    .long MB2_MAGIC
    .long MB2_ARCH_I386
    .long multiboot2_header_end - multiboot2_header
    .long -(MB2_MAGIC + MB2_ARCH_I386 + (multiboot2_header_end - multiboot2_header))

    // Information request tag: ask GRUB for the memory map (type 6).
    // size includes padding to keep 8-byte alignment.
    .short 1
    .short 0
    .long 16
    .long 6
    .long 0

    .short 0
    .short 0
    .long 8

multiboot2_header_end:

.section .boot.bss,"aw",@nobits
.align 16
boot_stack:
    .space 16384
boot_stack_top:

.align 4096
boot_pml4:
    .space 4096
boot_pdpt:
    .space 4096
boot_pd:
    .space 4096

.section .boot.rodata,"a"
.align 8
gdt64:
    .quad 0x0000000000000000
    .quad 0x00AF9A000000FFFF
    .quad 0x00AF92000000FFFF
gdt64_end:

gdt64_ptr:
    .word gdt64_end - gdt64 - 1
    .quad gdt64

.section .boot.text,"ax"
.code32
.global _start
.type _start, @function

_start:
    cli
    cmp $0x36d76289, %eax
    jne .hang

    mov $boot_stack_top, %esp
    mov %ebx, %edi

    // setup_page_tables clobbers %edi; preserve the mb2 info pointer.
    pushl %edi

    call setup_page_tables

    popl %edi

    mov $boot_pml4, %eax
    mov %eax, %cr3

    mov %cr4, %eax
    or $CR4_PAE, %eax
    mov %eax, %cr4

    mov $EFER_MSR, %ecx
    rdmsr
    or $EFER_LME, %eax
    wrmsr

    mov %cr0, %eax
    or $(CR0_PG | CR0_PE), %eax
    mov %eax, %cr0

    lgdt gdt64_ptr
    ljmp $0x08, $long_mode_entry

.hang:
    hlt
    jmp .hang

setup_page_tables:
    mov $boot_pml4, %edi
    mov $0, %eax
    mov $3072, %ecx
    rep stosl

    mov $boot_pdpt, %eax
    or $(PTE_P | PTE_RW), %eax
    mov $boot_pml4, %edi
    mov %eax, (%edi)
    mov %eax, 511*8(%edi)

    mov $boot_pd, %eax
    or $(PTE_P | PTE_RW), %eax
    mov $boot_pdpt, %edi
    mov %eax, (%edi)
    mov %eax, 510*8(%edi)

    mov $boot_pd, %edi
    xor %ebx, %ebx
    mov $512, %ecx

.map_2m:
    mov %ebx, %eax
    or $(PTE_P | PTE_RW | PTE_PS), %eax
    mov %eax, (%edi)
    add $8, %edi
    add $0x200000, %ebx
    loop .map_2m
    ret

.code64
long_mode_entry:
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss
    mov %ax, %fs
    mov %ax, %gs

    mov $boot_stack_top, %rsp
    mov %rdi, %rdi
    movabs $kmain, %rax
    call *%rax

.lhang:
    hlt
    jmp .lhang

.size _start, . - _start
