#include "nm/tss.h"

static struct nm_tss64 kernel_tss __attribute__((aligned(16)));

uint64_t tss_base_address(void)
{
    return (uint64_t)&kernel_tss;
}

void tss_init(void)
{
    kernel_tss.reserved0 = 0;
    kernel_tss.rsp0 = 0;
    kernel_tss.rsp1 = 0;
    kernel_tss.rsp2 = 0;
    kernel_tss.reserved1 = 0;
    kernel_tss.ist1 = 0;
    kernel_tss.ist2 = 0;
    kernel_tss.ist3 = 0;
    kernel_tss.ist4 = 0;
    kernel_tss.ist5 = 0;
    kernel_tss.ist6 = 0;
    kernel_tss.ist7 = 0;
    kernel_tss.reserved2 = 0;
    kernel_tss.reserved3 = 0;
    kernel_tss.iopb = sizeof(kernel_tss);

    uint16_t tss_sel = 0x18;
    __asm__ volatile("ltr %0" : : "m"(tss_sel) : "memory");
}
