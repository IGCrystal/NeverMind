#ifndef NM_DRIVER_H
#define NM_DRIVER_H

#include <stdint.h>

struct nm_device {
    const char *name;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t class_code;
    uint8_t subclass;
    uint8_t prog_if;
    uint8_t bus;
    uint8_t slot;
    uint8_t func;
    uint32_t bar0;
};

struct nm_driver_ops {
    int (*probe)(const struct nm_device *dev);
    int (*remove)(const struct nm_device *dev);
    int64_t (*read)(void *buf, uint64_t len);
    int64_t (*write)(const void *buf, uint64_t len);
    int (*ioctl)(uint64_t req, uint64_t arg);
};

#endif
