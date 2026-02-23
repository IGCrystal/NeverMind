#ifndef NM_PCI_H
#define NM_PCI_H

#include <stddef.h>
#include <stdint.h>

#include "nm/driver.h"

#define NM_PCI_MAX_DEVICES 64

void pci_init(void);
size_t pci_device_count(void);
const struct nm_device *pci_get_device(size_t index);
const struct nm_device *pci_find_device(uint16_t vendor, uint16_t device);

#ifdef NEVERMIND_HOST_TEST
void pci_test_inject(const struct nm_device *devices, size_t count);
#endif

#endif
