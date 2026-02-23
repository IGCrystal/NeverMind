#include <stdint.h>

#include "nm/idt.h"

extern void nm_isr_ud(void);
extern void nm_isr_df(void);
extern void nm_isr_gp(void);
extern void nm_isr_pf(void);

extern void nm_isr_irq0(void);
extern void nm_isr_irq1(void);

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

static void idt_set_gate(int vec, uint64_t handler, uint8_t type_attr, uint8_t ist)
{
    if (vec < 0 || vec >= 256) {
        return;
    }

    idt[vec].offset_low = (uint16_t)(handler & 0xFFFFULL);
    idt[vec].selector = 0x08;
    idt[vec].ist = ist;
    idt[vec].type_attr = type_attr;
    idt[vec].offset_mid = (uint16_t)((handler >> 16) & 0xFFFFULL);
    idt[vec].offset_high = (uint32_t)((handler >> 32) & 0xFFFFFFFFULL);
    idt[vec].zero = 0;
}

void idt_init(void)
{
    for (int i = 0; i < 256; i++) {
        idt_set_gate(i, 0, 0, 0);
    }

    idt_set_gate(6, (uint64_t)nm_isr_ud, 0x8E, 0);
    idt_set_gate(8, (uint64_t)nm_isr_df, 0x8E, 0);
    idt_set_gate(13, (uint64_t)nm_isr_gp, 0x8E, 0);
    idt_set_gate(14, (uint64_t)nm_isr_pf, 0x8E, 0);

    // PIC remapped IRQs (timer/keyboard)
    idt_set_gate(32, (uint64_t)nm_isr_irq0, 0x8E, 0);
    idt_set_gate(33, (uint64_t)nm_isr_irq1, 0x8E, 0);

    struct idt_ptr idtp = {
        .limit = (uint16_t)(sizeof(idt) - 1),
        .base = (uint64_t)&idt,
    };

    __asm__ volatile("lidt %0" : : "m"(idtp));
}
