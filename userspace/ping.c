#include <stdint.h>
#include <stdio.h>

#include "nm/net.h"

int main(void)
{
    uint32_t dst = 0xC0A80101;
    const char payload[] = "NeverMind ping";

    int rc = icmp_send_echo(dst, 1, 1, payload, (uint16_t)(sizeof(payload) - 1));
    if (rc == 0) {
        puts("ping: echo request sent");
        return 0;
    }
    puts("ping: failed");
    return 1;
}
