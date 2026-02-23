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
static volatile uint32_t irq_lock_word;

static inline void irq_lock(void)
{
    while (__sync_lock_test_and_set(&irq_lock_word, 1U) != 0U) {
        __asm__ volatile("pause");
    }
}

static inline void irq_unlock(void)
{
    __sync_lock_release(&irq_lock_word);
}

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
    irq_lock_word = 0;
    irq_lock();
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
    irq_unlock();
}

int irq_register(int irq, nm_irq_top_half_t top_half, nm_irq_bottom_half_t bottom_half, void *ctx,
                 const char *name)
{
    if (irq < 0 || irq >= NM_MAX_IRQ || top_half == 0) {
        return -1;
    }
    irq_lock();
    irq_table[irq].used = true;
    irq_table[irq].top_half = top_half;
    irq_table[irq].bottom_half = bottom_half;
    irq_table[irq].ctx = ctx;
    irq_table[irq].name = name;
    irq_table[irq].hit_count = 0;
    irq_unlock();
    return 0;
}

int irq_unregister(int irq)
{
    if (irq < 0 || irq >= NM_MAX_IRQ) {
        return -1;
    }
    irq_lock();
    irq_table[irq].used = false;
    irq_table[irq].top_half = 0;
    irq_table[irq].bottom_half = 0;
    irq_table[irq].ctx = 0;
    irq_table[irq].name = 0;
    irq_table[irq].hit_count = 0;
    irq_unlock();
    return 0;
}

int irq_handle(int irq)
{
    if (irq < 0 || irq >= NM_MAX_IRQ) {
        return -1;
    }

    irq_lock();
    if (!irq_table[irq].used || irq_table[irq].top_half == 0) {
        irq_unlock();
        return -1;
    }

    nm_irq_top_half_t top_half = irq_table[irq].top_half;
    nm_irq_bottom_half_t bottom_half = irq_table[irq].bottom_half;
    void *ctx = irq_table[irq].ctx;
    irq_table[irq].hit_count++;
    irq_unlock();

    top_half(irq, ctx);

    if (bottom_half) {
        irq_lock();
        (void)bh_enqueue(bottom_half, ctx);
        irq_unlock();
    }

    return 0;
}

void irq_run_bottom_halves(void)
{
    for (;;) {
        irq_lock();
        if (bh_head == bh_tail) {
            irq_unlock();
            break;
        }

        struct bh_item item = bh_queue[bh_head];
        bh_head = (bh_head + 1) % NM_BH_QUEUE_CAP;
        irq_unlock();

        if (item.fn) {
            item.fn(item.ctx);
        }
    }
}

const struct nm_irq_desc *irq_get_desc(int irq)
{
    if (irq < 0 || irq >= NM_MAX_IRQ) {
        return 0;
    }

    irq_lock();
    const struct nm_irq_desc *desc = irq_table[irq].used ? &irq_table[irq] : 0;
    irq_unlock();
    return desc;
}
