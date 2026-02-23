#include "nm/net.h"

#include <stdint.h>

void icmp_input(uint32_t src_ip, uint32_t dst_ip, const uint8_t *payload, uint16_t len);
void udp_input(uint32_t src_ip, uint32_t dst_ip, const uint8_t *payload, uint16_t len);
void tcp_input(uint32_t src_ip, uint32_t dst_ip, const uint8_t *payload, uint16_t len);

void ipv4_input(const uint8_t *packet, uint16_t len)
{
    if (packet == 0 || len < 20) {
        return;
    }

    uint8_t ihl = (uint8_t)((packet[0] & 0x0F) * 4);
    if (ihl < 20 || len < ihl) {
        return;
    }

    uint8_t proto = packet[9];
    uint32_t src_ip = ((uint32_t)packet[12] << 24) | ((uint32_t)packet[13] << 16) |
                      ((uint32_t)packet[14] << 8) | (uint32_t)packet[15];
    uint32_t dst_ip = ((uint32_t)packet[16] << 24) | ((uint32_t)packet[17] << 16) |
                      ((uint32_t)packet[18] << 8) | (uint32_t)packet[19];

    const uint8_t *payload = packet + ihl;
    uint16_t payload_len = (uint16_t)(len - ihl);

    if (proto == NM_IP_PROTO_ICMP) {
        icmp_input(src_ip, dst_ip, payload, payload_len);
    } else if (proto == NM_IP_PROTO_UDP) {
        udp_input(src_ip, dst_ip, payload, payload_len);
    } else if (proto == NM_IP_PROTO_TCP) {
        tcp_input(src_ip, dst_ip, payload, payload_len);
    }
}
