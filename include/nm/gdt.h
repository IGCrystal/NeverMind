#ifndef NM_GDT_H
#define NM_GDT_H

#include <stdint.h>

struct __attribute__((packed)) gdt_ptr {
    uint16_t limit;
    uint64_t base;
};

void gdt_init(void);

#endif
