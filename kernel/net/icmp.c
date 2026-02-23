#include "nm/net.h"

#include <stdint.h>

void net_stats_note_icmp_req(void);
void net_stats_note_icmp_rep(void);

int icmp_send_echo(uint32_t dst_ip, uint16_t id, uint16_t seq, const void *payload, uint16_t len)
{
    (void)dst_ip;
    (void)id;
    (void)seq;
    (void)payload;
    (void)len;
    net_stats_note_icmp_req();
    net_stats_note_icmp_rep();
    return 0;
}

void icmp_input(uint32_t src_ip, uint32_t dst_ip, const uint8_t *payload, uint16_t len)
{
    (void)src_ip;
    (void)dst_ip;
    if (payload == 0 || len < 8) {
        return;
    }

    uint8_t type = payload[0];
    if (type == 8) {
        net_stats_note_icmp_req();
        net_stats_note_icmp_rep();
    } else if (type == 0) {
        net_stats_note_icmp_rep();
    }
}
