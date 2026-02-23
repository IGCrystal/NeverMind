#include "nm/pic.h"

#include <stdint.h>

#include "nm/io.h"

#define PIC1_COMMAND 0x20
#define PIC1_DATA 0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA 0xA1

#define PIC_EOI 0x20

static void pic_remap(uint8_t offset1, uint8_t offset2)
{
    uint8_t a1 = inb(PIC1_DATA);
    uint8_t a2 = inb(PIC2_DATA);

    outb(PIC1_COMMAND, 0x11);
    io_wait();
    outb(PIC2_COMMAND, 0x11);
    io_wait();

    outb(PIC1_DATA, offset1);
    io_wait();
    outb(PIC2_DATA, offset2);
    io_wait();

    outb(PIC1_DATA, 0x04);
    io_wait();
    outb(PIC2_DATA, 0x02);
    io_wait();

    outb(PIC1_DATA, 0x01);
    io_wait();
    outb(PIC2_DATA, 0x01);
    io_wait();

    outb(PIC1_DATA, a1);
    outb(PIC2_DATA, a2);
}

void pic_set_mask(uint8_t irq_line, int masked)
{
    uint16_t port;
    uint8_t value;

    if (irq_line < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq_line -= 8;
    }

    value = inb(port);
    if (masked) {
        value |= (uint8_t)(1U << irq_line);
    } else {
        value &= (uint8_t)~(1U << irq_line);
    }
    outb(port, value);
}

void pic_send_eoi(uint8_t irq_line)
{
    if (irq_line >= 8) {
        outb(PIC2_COMMAND, PIC_EOI);
    }
    outb(PIC1_COMMAND, PIC_EOI);
}

void pic_init(void)
{
    // Remap PIC IRQs to vectors 32..47
    pic_remap(0x20, 0x28);

    // Mask everything, then unmask IRQ0(timer) and IRQ1(keyboard).
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);

    pic_set_mask(0, 0);
    pic_set_mask(1, 0);
}
