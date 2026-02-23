#include <stdint.h>

#include "nm/tss.h"

struct __attribute__((packed)) tss64 {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb;
};

static struct tss64 kernel_tss;

uint64_t tss_base_address(void)
{
    return (uint64_t)&kernel_tss;
}

void tss_init(void)
{
    kernel_tss.rsp0 = 0;
    kernel_tss.iopb = sizeof(kernel_tss);
    __asm__ volatile("ltr %0" : : "r"((uint16_t)0x18));
}
