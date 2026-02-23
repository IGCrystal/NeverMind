#include "nm/pci.h"

#include <stddef.h>
#include <stdint.h>

#include "nm/io.h"

static struct nm_device pci_devices[NM_PCI_MAX_DEVICES];
static size_t pci_count;

static uint32_t pci_cfg_addr(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off)
{
    return (1U << 31) | ((uint32_t)bus << 16) | ((uint32_t)slot << 11) | ((uint32_t)func << 8) |
           (off & 0xFC);
}

static uint32_t pci_cfg_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off)
{
#ifdef NEVERMIND_HOST_TEST
    (void)bus;
    (void)slot;
    (void)func;
    (void)off;
    return 0xFFFFFFFFU;
#else
    outb(0xCF8, (uint8_t)(pci_cfg_addr(bus, slot, func, off) & 0xFF));
    outb(0xCF8 + 1, (uint8_t)((pci_cfg_addr(bus, slot, func, off) >> 8) & 0xFF));
    outb(0xCF8 + 2, (uint8_t)((pci_cfg_addr(bus, slot, func, off) >> 16) & 0xFF));
    outb(0xCF8 + 3, (uint8_t)((pci_cfg_addr(bus, slot, func, off) >> 24) & 0xFF));

    uint32_t value = 0;
    value |= (uint32_t)inb(0xCFC);
    value |= (uint32_t)inb(0xCFC + 1) << 8;
    value |= (uint32_t)inb(0xCFC + 2) << 16;
    value |= (uint32_t)inb(0xCFC + 3) << 24;
    return value;
#endif
}

void pci_init(void)
{
#ifdef NEVERMIND_HOST_TEST
    return;
#else
    pci_count = 0;
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            uint32_t vdid = pci_cfg_read32((uint8_t)bus, slot, 0, 0x00);
            uint16_t vendor = (uint16_t)(vdid & 0xFFFF);
            uint16_t device = (uint16_t)((vdid >> 16) & 0xFFFF);
            if (vendor == 0xFFFF) {
                continue;
            }

            if (pci_count >= NM_PCI_MAX_DEVICES) {
                return;
            }

            uint32_t class_reg = pci_cfg_read32((uint8_t)bus, slot, 0, 0x08);
            uint32_t bar0 = pci_cfg_read32((uint8_t)bus, slot, 0, 0x10);

            pci_devices[pci_count] = (struct nm_device){
                .name = "pci-dev",
                .vendor_id = vendor,
                .device_id = device,
                .class_code = (uint8_t)((class_reg >> 24) & 0xFF),
                .subclass = (uint8_t)((class_reg >> 16) & 0xFF),
                .prog_if = (uint8_t)((class_reg >> 8) & 0xFF),
                .bus = (uint8_t)bus,
                .slot = slot,
                .func = 0,
                .bar0 = bar0,
            };
            pci_count++;
        }
    }
#endif
}

size_t pci_device_count(void)
{
    return pci_count;
}

const struct nm_device *pci_get_device(size_t index)
{
    if (index >= pci_count) {
        return 0;
    }
    return &pci_devices[index];
}

const struct nm_device *pci_find_device(uint16_t vendor, uint16_t device)
{
    for (size_t i = 0; i < pci_count; i++) {
        if (pci_devices[i].vendor_id == vendor && pci_devices[i].device_id == device) {
            return &pci_devices[i];
        }
    }
    return 0;
}

#ifdef NEVERMIND_HOST_TEST
void pci_test_inject(const struct nm_device *devices, size_t count)
{
    if (count > NM_PCI_MAX_DEVICES) {
        count = NM_PCI_MAX_DEVICES;
    }
    for (size_t i = 0; i < count; i++) {
        pci_devices[i] = devices[i];
    }
    pci_count = count;
}
#endif
