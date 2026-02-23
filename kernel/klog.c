#include "nm/klog.h"

#include <stddef.h>

#define KLOG_CAP 8192

static char ring[KLOG_CAP];
static size_t head;
static size_t tail;
static int ready;

void klog_init(void)
{
    head = 0;
    tail = 0;
    ready = 1;
}

void klog_putc(char c)
{
    if (!ready) {
        return;
    }

    ring[head] = c;
    head = (head + 1) % KLOG_CAP;
    if (head == tail) {
        tail = (tail + 1) % KLOG_CAP;
    }
}

void klog_write(const char *msg)
{
    if (!msg) {
        return;
    }
    while (*msg != '\0') {
        klog_putc(*msg++);
    }
}

size_t klog_read(char *out, size_t cap)
{
    if (!ready || !out || cap == 0) {
        return 0;
    }

    size_t n = 0;
    size_t p = tail;
    while (p != head && n + 1 < cap) {
        out[n++] = ring[p];
        p = (p + 1) % KLOG_CAP;
    }
    out[n] = '\0';
    return n;
}
