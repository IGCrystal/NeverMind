#include "nm/irq.h"

#include <stddef.h>

#define NM_BH_QUEUE_CAP 256

struct bh_item {
    nm_irq_bottom_half_t fn;
    void *ctx;
};

static struct nm_irq_desc irq_table[NM_MAX_IRQ];
static struct bh_item bh_queue[NM_BH_QUEUE_CAP];
static size_t bh_head;
static size_t bh_tail;

static int bh_enqueue(nm_irq_bottom_half_t fn, void *ctx)
{
    size_t next = (bh_tail + 1) % NM_BH_QUEUE_CAP;
    if (next == bh_head) {
        return -1;
    }
    bh_queue[bh_tail].fn = fn;
    bh_queue[bh_tail].ctx = ctx;
    bh_tail = next;
    return 0;
}

void irq_init(void)
{
    for (int i = 0; i < NM_MAX_IRQ; i++) {
        irq_table[i].used = false;
        irq_table[i].irq = i;
        irq_table[i].top_half = 0;
        irq_table[i].bottom_half = 0;
        irq_table[i].ctx = 0;
        irq_table[i].name = 0;
        irq_table[i].hit_count = 0;
    }
    bh_head = 0;
    bh_tail = 0;
}

int irq_register(int irq, nm_irq_top_half_t top_half, nm_irq_bottom_half_t bottom_half, void *ctx,
                 const char *name)
{
    if (irq < 0 || irq >= NM_MAX_IRQ || top_half == 0) {
        return -1;
    }
    irq_table[irq].used = true;
    irq_table[irq].top_half = top_half;
    irq_table[irq].bottom_half = bottom_half;
    irq_table[irq].ctx = ctx;
    irq_table[irq].name = name;
    irq_table[irq].hit_count = 0;
    return 0;
}

int irq_unregister(int irq)
{
    if (irq < 0 || irq >= NM_MAX_IRQ) {
        return -1;
    }
    irq_table[irq].used = false;
    irq_table[irq].top_half = 0;
    irq_table[irq].bottom_half = 0;
    irq_table[irq].ctx = 0;
    irq_table[irq].name = 0;
    irq_table[irq].hit_count = 0;
    return 0;
}

int irq_handle(int irq)
{
    if (irq < 0 || irq >= NM_MAX_IRQ || !irq_table[irq].used || irq_table[irq].top_half == 0) {
        return -1;
    }

    irq_table[irq].hit_count++;
    irq_table[irq].top_half(irq, irq_table[irq].ctx);
    if (irq_table[irq].bottom_half) {
        (void)bh_enqueue(irq_table[irq].bottom_half, irq_table[irq].ctx);
    }
    return 0;
}

void irq_run_bottom_halves(void)
{
    while (bh_head != bh_tail) {
        struct bh_item item = bh_queue[bh_head];
        bh_head = (bh_head + 1) % NM_BH_QUEUE_CAP;
        if (item.fn) {
            item.fn(item.ctx);
        }
    }
}

const struct nm_irq_desc *irq_get_desc(int irq)
{
    if (irq < 0 || irq >= NM_MAX_IRQ || !irq_table[irq].used) {
        return 0;
    }
    return &irq_table[irq];
}
