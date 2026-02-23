#ifndef NM_KLOG_H
#define NM_KLOG_H

#include <stddef.h>

void klog_init(void);
void klog_putc(char c);
void klog_write(const char *msg);
size_t klog_read(char *out, size_t cap);

#endif
