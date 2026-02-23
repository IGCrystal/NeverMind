#include <stdio.h>

#include "nm/klog.h"

int main(void)
{
    char buf[8192];
    size_t n = klog_read(buf, sizeof(buf));
    if (n == 0) {
        puts("dmesg: no logs");
        return 0;
    }
    puts(buf);
    return 0;
}
