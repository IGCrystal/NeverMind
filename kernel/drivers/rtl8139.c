#include "nm/rtl8139.h"

#include <stdint.h>

#include "nm/pci.h"

static const struct nm_device *rtl_dev;

int rtl8139_init(void)
{
    rtl_dev = pci_find_device(0x10EC, 0x8139);
    if (rtl_dev == 0) {
        return -1;
    }
    return 0;
}

int64_t rtl8139_send(const void *frame, uint64_t len)
{
    if (rtl_dev == 0 || frame == 0 || len == 0 || len > 1518) {
        return -1;
    }
    return (int64_t)len;
}

int64_t rtl8139_recv(void *frame, uint64_t cap)
{
    (void)frame;
    (void)cap;
    if (rtl_dev == 0) {
        return -1;
    }
    return 0;
}
