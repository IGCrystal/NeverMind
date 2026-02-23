#ifndef NM_RTL8139_H
#define NM_RTL8139_H

#include <stdint.h>

int rtl8139_init(void);
int64_t rtl8139_send(const void *frame, uint64_t len);
int64_t rtl8139_recv(void *frame, uint64_t cap);

#endif
