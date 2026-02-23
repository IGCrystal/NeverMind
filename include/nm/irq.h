#ifndef NM_IRQ_H
#define NM_IRQ_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define NM_MAX_IRQ 256

typedef void (*nm_irq_top_half_t)(int irq, void *ctx);
typedef void (*nm_irq_bottom_half_t)(void *ctx);

struct nm_irq_desc {
    bool used;
    int irq;
    nm_irq_top_half_t top_half;
    nm_irq_bottom_half_t bottom_half;
    void *ctx;
    const char *name;
    uint64_t hit_count;
};

void irq_init(void);
int irq_register(int irq, nm_irq_top_half_t top_half, nm_irq_bottom_half_t bottom_half, void *ctx,
                 const char *name);
int irq_unregister(int irq);
int irq_handle(int irq);
void irq_run_bottom_halves(void);
const struct nm_irq_desc *irq_get_desc(int irq);

#endif
