#include <stdint.h>

#include "nm/console.h"
#include "nm/gdt.h"
#include "nm/idt.h"
#include "nm/mm.h"
#include "nm/tss.h"

static void console_write_u64(uint64_t value)
{
    char buf[32];
    int idx = 0;

    if (value == 0) {
        console_putc('0');
        return;
    }

    while (value > 0 && idx < (int)sizeof(buf)) {
        buf[idx++] = (char)('0' + (value % 10));
        value /= 10;
    }

    while (idx > 0) {
        console_putc(buf[--idx]);
    }
}

static void kernel_banner(void)
{
    console_write("NeverMind kernel (M2)\n");
    console_write("arch: x86_64\n");
    console_write("boot: BIOS+UEFI via GRUB multiboot2\n");
}

void kmain(uint64_t mb2_info)
{
    console_init();
    console_write("[00.000000] early console ready\n");

    gdt_init();
    console_write("[00.000100] gdt ready\n");

    idt_init();
    console_write("[00.000200] idt ready\n");

    tss_init();
    console_write("[00.000300] tss ready\n");

    mm_init(mb2_info);
    struct nm_mm_stats stats = pmm_get_stats();
    console_write("[00.000400] mm ready: free_pages=");
    console_write_u64(stats.free_frames);
    console_write(" used_pages=");
    console_write_u64(stats.used_frames);
    console_write("\n");

    kernel_banner();
    console_write("[00.001000] NeverMind: M2 mm boot ok\n");

    for (;;) {
        __asm__ volatile("hlt");
    }
}
