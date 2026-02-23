#include "nm/timer.h"

#include <stdint.h>

#include "nm/io.h"
#include "nm/irq.h"

#define PIT_CH0 0x40
#define PIT_CMD 0x43
#define PIT_BASE_HZ 1193182U

static uint64_t g_pit_ticks;

static void pit_irq_top(int irq, void *ctx)
{
    (void)irq;
    (void)ctx;
    g_pit_ticks++;
}

void pit_init(uint32_t hz)
{
    if (hz == 0) {
        hz = 100;
    }
    uint16_t divisor = (uint16_t)(PIT_BASE_HZ / hz);

#ifndef NEVERMIND_HOST_TEST
    outb(PIT_CMD, 0x36);
    outb(PIT_CH0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CH0, (uint8_t)((divisor >> 8) & 0xFF));
#endif

    (void)irq_register(32, pit_irq_top, 0, 0, "pit");
}

uint64_t pit_ticks(void)
{
    return g_pit_ticks;
}
