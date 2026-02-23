#include <stdint.h>

#include "nm/gdt.h"
#include "nm/tss.h"

struct __attribute__((packed)) gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_mid;
    uint8_t access;
    uint8_t gran;
    uint8_t base_high;
};

struct __attribute__((packed)) tss_desc {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_mid1;
    uint8_t access;
    uint8_t gran;
    uint8_t base_mid2;
    uint32_t base_high;
    uint32_t reserved;
};

static struct {
    struct gdt_entry null;
    struct gdt_entry code;
    struct gdt_entry data;
    struct tss_desc tss;
} __attribute__((packed, aligned(16))) gdt;

void gdt_init(void)
{
    gdt.code = (struct gdt_entry){
        .limit_low = 0xFFFF,
        .base_low = 0,
        .base_mid = 0,
        .access = 0x9A,
        .gran = 0xAF,
        .base_high = 0,
    };

    gdt.data = (struct gdt_entry){
        .limit_low = 0xFFFF,
        .base_low = 0,
        .base_mid = 0,
        .access = 0x92,
        .gran = 0xAF,
        .base_high = 0,
    };

    uint64_t base = tss_base_address();
    uint32_t limit = (uint32_t)(sizeof(struct nm_tss64) - 1);

    gdt.tss.limit_low = limit & 0xFFFF;
    gdt.tss.base_low = base & 0xFFFF;
    gdt.tss.base_mid1 = (base >> 16) & 0xFF;
    gdt.tss.access = 0x89;
    gdt.tss.gran = (limit >> 16) & 0x0F;
    gdt.tss.base_mid2 = (base >> 24) & 0xFF;
    gdt.tss.base_high = (uint32_t)(base >> 32);
    gdt.tss.reserved = 0;

    struct gdt_ptr gp = {
        .limit = (uint16_t)(sizeof(gdt) - 1),
        .base = (uint64_t)&gdt,
    };

    __asm__ volatile("lgdt %0" : : "m"(gp));

    __asm__ volatile(
        "pushq $0x08\n"
        "lea 1f(%%rip), %%rax\n"
        "pushq %%rax\n"
        "lretq\n"
        "1:\n"
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%ss\n"
        :
        :
        : "rax", "ax", "memory");
}
