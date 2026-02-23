#include <stdint.h>

#include "nm/irq.h"
#include "nm/pic.h"

void nm_irq_isr(uint64_t vector)
{
    // Vectors 32..47 are remapped PIC IRQs.
    if (vector >= 32 && vector < 48) {
        (void)irq_handle((int)vector);
        pic_send_eoi((uint8_t)(vector - 32));
        return;
    }

    (void)irq_handle((int)vector);
}
