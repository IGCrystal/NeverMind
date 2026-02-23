#include <stdint.h>

#include "nm/console.h"
#include "nm/gdt.h"
#include "nm/idt.h"
#include "nm/tss.h"

static void kernel_banner(void)
{
    console_write("NeverMind kernel (M1)\n");
    console_write("arch: x86_64\n");
    console_write("boot: BIOS+UEFI via GRUB multiboot2\n");
}

void kmain(uint64_t mb2_info)
{
    (void)mb2_info;

    console_init();
    console_write("[00.000000] early console ready\n");

    gdt_init();
    console_write("[00.000100] gdt ready\n");

    idt_init();
    console_write("[00.000200] idt ready\n");

    tss_init();
    console_write("[00.000300] tss ready\n");

    kernel_banner();
    console_write("[00.001000] NeverMind: M1 boot ok\n");

    for (;;) {
        __asm__ volatile("hlt");
    }
}
