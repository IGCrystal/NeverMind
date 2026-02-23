#include <stdint.h>

#include "nm/console.h"
#include "nm/fs.h"
#include "nm/gdt.h"
#include "nm/idt.h"
#include "nm/irq.h"
#include "nm/keyboard.h"
#include "nm/klog.h"
#include "nm/mm.h"
#include "nm/net.h"
#include "nm/pci.h"
#include "nm/proc.h"
#include "nm/rtl8139.h"
#include "nm/syscall.h"
#include "nm/timer.h"
#include "nm/tss.h"
#include "nm/userspace.h"

static void idle_thread(void *arg)
{
    (void)arg;
    for (;;) {
        __asm__ volatile("hlt");
    }
}

static void worker_thread(void *arg)
{
    (void)arg;
    for (;;) {
        __asm__ volatile("pause");
    }
}

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
    console_write("NeverMind kernel (M8)\n");
    console_write("arch: x86_64\n");
    console_write("boot: BIOS+UEFI via GRUB multiboot2\n");
}

static void cpu_enable_sse(void)
{
    uint64_t cr0;
    uint64_t cr4;

    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~(1ULL << 2);  // CR0.EM = 0 (enable FPU)
    cr0 &= ~(1ULL << 3);  // CR0.TS = 0 (no task switch trapping)
    cr0 |= (1ULL << 1);   // CR0.MP = 1
    __asm__ volatile("mov %0, %%cr0" : : "r"(cr0) : "memory");

    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1ULL << 9);   // CR4.OSFXSR = 1 (enable FXSAVE/FXRSTOR)
    cr4 |= (1ULL << 10);  // CR4.OSXMMEXCPT = 1
    __asm__ volatile("mov %0, %%cr4" : : "r"(cr4) : "memory");

    __asm__ volatile("fninit");
}

void kmain(uint64_t mb2_info)
{
    klog_init();
    console_init();
    console_write("[00.000000] early console ready\n");

    gdt_init();
    console_write("[00.000100] gdt ready\n");

    cpu_enable_sse();

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

    proc_init();
    (void)task_create_kernel_thread("idle/0", idle_thread, 0);
    (void)task_create_kernel_thread("kworker/0", worker_thread, 0);
    sched_init(NM_SCHED_RR);
    console_write("[00.000500] proc+sched ready: policy=RR\n");

    syscall_init();
    console_write("[00.000600] syscall ready\n");

    fs_init();
    if (fs_mount_root(tmpfs_filesystem()) == 0) {
        console_write("[00.000700] fs ready: root=tmpfs\n");
    } else {
        console_write("[00.000700] fs ready: mount failed\n");
    }

    irq_init();
    pit_init(100);
    keyboard_init();
    pci_init();
    if (rtl8139_init() == 0) {
        console_write("[00.000800] drivers ready: pit/kbd/pci/rtl8139\n");
    } else {
        console_write("[00.000800] drivers ready: pit/kbd/pci (rtl8139 missing)\n");
    }

    net_init();
    console_write("[00.000900] net ready: arp/ipv4/icmp/udp/tcp/socket\n");

    userspace_init();

    kernel_banner();
    console_write("[00.001000] NeverMind: M8 hardening+ci ready\n");

    for (;;) {
        __asm__ volatile("hlt");
    }
}
