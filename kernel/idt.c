#include <stdint.h>

#include "nm/idt.h"

struct __attribute__((packed)) idt_gate {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
};

struct __attribute__((packed)) idt_ptr {
    uint16_t limit;
    uint64_t base;
};

static struct idt_gate idt[256];

void idt_init(void)
{
    struct idt_ptr idtp = {
        .limit = (uint16_t)(sizeof(idt) - 1),
        .base = (uint64_t)&idt,
    };

    __asm__ volatile("lidt %0" : : "m"(idtp));
}
