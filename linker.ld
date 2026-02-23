OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

KERNEL_PHYS_BASE = 0x00100000;
KERNEL_VIRT_BASE = 0xFFFF800000000000;

SECTIONS
{
    . = KERNEL_PHYS_BASE;

    .boot : ALIGN(4K)
    {
        KEEP(*(.multiboot2))
        *(.boot.text*)
        *(.boot.rodata*)
        *(.boot.data*)
        *(.boot.bss*)
    }

    . += KERNEL_VIRT_BASE;

    .text : ALIGN(4K) AT(ADDR(.text) - KERNEL_VIRT_BASE)
    {
        *(.text*)
    }

    .rodata : ALIGN(4K) AT(ADDR(.rodata) - KERNEL_VIRT_BASE)
    {
        *(.rodata*)
    }

    .data : ALIGN(4K) AT(ADDR(.data) - KERNEL_VIRT_BASE)
    {
        *(.data*)
    }

    .bss : ALIGN(4K) AT(ADDR(.bss) - KERNEL_VIRT_BASE)
    {
        *(COMMON)
        *(.bss*)
    }

    /DISCARD/ :
    {
        *(.eh_frame)
        *(.note*)
        *(.comment)
    }
}
