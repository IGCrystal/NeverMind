#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "nm/irq.h"
#include "nm/pci.h"

static int top_hits;
static int bottom_hits;

static void test_irq_top(int irq, void *ctx)
{
    (void)irq;
    int *val = (int *)ctx;
    top_hits += *val;
}

static void test_irq_bottom(void *ctx)
{
    int *val = (int *)ctx;
    bottom_hits += *val;
}

static void test_irq_flow(void)
{
    irq_init();
    int step = 2;
    assert(irq_register(40, test_irq_top, test_irq_bottom, &step, "test-irq") == 0);
    assert(irq_handle(40) == 0);
    assert(top_hits == 2);
    assert(bottom_hits == 0);

    irq_run_bottom_halves();
    assert(bottom_hits == 2);

    const struct nm_irq_desc *d = irq_get_desc(40);
    assert(d != 0);
    assert(d->hit_count == 1);
    assert(irq_unregister(40) == 0);
}

static void test_pci_lookup(void)
{
    struct nm_device fake[2] = {
        {.name = "rtl", .vendor_id = 0x10EC, .device_id = 0x8139, .class_code = 0x02},
        {.name = "other", .vendor_id = 0x1234, .device_id = 0x5678, .class_code = 0x01},
    };
    pci_test_inject(fake, 2);

    assert(pci_device_count() == 2);
    const struct nm_device *d0 = pci_get_device(0);
    assert(d0 != 0);
    assert(d0->vendor_id == 0x10EC);

    const struct nm_device *rtl = pci_find_device(0x10EC, 0x8139);
    assert(rtl != 0);
    assert(rtl->class_code == 0x02);
}

int main(void)
{
    test_irq_flow();
    test_pci_lookup();
    puts("test_irq_pci: PASS");
    return 0;
}
