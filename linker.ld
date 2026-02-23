OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

KERNEL_PHYS_BASE = 0x00100000;
KERNEL_VIRT_BASE = 0xFFFFFFFF80000000;

PHDRS
{
    boot_text PT_LOAD FLAGS(5);
    boot_data PT_LOAD FLAGS(6);
    text PT_LOAD FLAGS(5);
    data PT_LOAD FLAGS(6);
}

SECTIONS
{
    . = KERNEL_PHYS_BASE;

    __kernel_phys_start = .;

    .boot.text : ALIGN(4K)
    {
        KEEP(*(.multiboot2))
        *(.boot.text*)
        *(.boot.rodata*)
    } :boot_text

    .boot.data : ALIGN(4K)
    {
        *(.boot.data*)
        *(.boot.bss*)
    } :boot_data

    . += KERNEL_VIRT_BASE;

    .text ALIGN(4K) : AT(ADDR(.text) - KERNEL_VIRT_BASE)
    {
        *(.text*)
    } :text

    .rodata ALIGN(4K) : AT(ADDR(.rodata) - KERNEL_VIRT_BASE)
    {
        *(.rodata*)
    } :text

    .data ALIGN(4K) : AT(ADDR(.data) - KERNEL_VIRT_BASE)
    {
        *(.data*)
    } :data

    .bss ALIGN(4K) : AT(ADDR(.bss) - KERNEL_VIRT_BASE)
    {
        *(COMMON)
        *(.bss*)
    } :data

    __kernel_phys_end = LOADADDR(.bss) + SIZEOF(.bss);

    /DISCARD/ :
    {
        *(.eh_frame)
        *(.note*)
        *(.comment)
    }
}
