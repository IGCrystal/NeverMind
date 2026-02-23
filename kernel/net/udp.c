#include "nm/net.h"

#include <stdbool.h>
#include <stdint.h>

void net_stats_note_udp_rx(void);
void net_stats_note_udp_tx(void);

#define UDP_PORT_MAX 64
#define UDP_QUEUE_CAP 16
#define UDP_PAYLOAD_MAX 1500

struct udp_msg {
    bool used;
    uint32_t src_ip;
    uint16_t src_port;
    uint16_t len;
    uint8_t payload[UDP_PAYLOAD_MAX];
};

struct udp_port {
    bool used;
    uint16_t port;
    struct udp_msg q[UDP_QUEUE_CAP];
};

static struct udp_port ports[UDP_PORT_MAX];

static struct udp_port *find_port(uint16_t port)
{
    for (int i = 0; i < UDP_PORT_MAX; i++) {
        if (ports[i].used && ports[i].port == port) {
            return &ports[i];
        }
    }
    return 0;
}

int udp_bind(uint16_t port)
{
    if (port == 0) {
        return -1;
    }
    if (find_port(port)) {
        return 0;
    }
    for (int i = 0; i < UDP_PORT_MAX; i++) {
        if (!ports[i].used) {
            ports[i].used = true;
            ports[i].port = port;
            for (int j = 0; j < UDP_QUEUE_CAP; j++) {
                ports[i].q[j].used = false;
            }
            return 0;
        }
    }
    return -1;
}

int udp_unbind(uint16_t port)
{
    struct udp_port *p = find_port(port);
    if (!p) {
        return -1;
    }
    p->used = false;
    return 0;
}

static int udp_deliver(uint16_t dst_port, uint32_t src_ip, uint16_t src_port, const void *payload,
                       uint16_t len)
{
    struct udp_port *p = find_port(dst_port);
    if (!p) {
        return -1;
    }

    for (int i = 0; i < UDP_QUEUE_CAP; i++) {
        if (!p->q[i].used) {
            p->q[i].used = true;
            p->q[i].src_ip = src_ip;
            p->q[i].src_port = src_port;
            p->q[i].len = len > UDP_PAYLOAD_MAX ? UDP_PAYLOAD_MAX : len;
            for (uint16_t j = 0; j < p->q[i].len; j++) {
                p->q[i].payload[j] = ((const uint8_t *)payload)[j];
            }
            net_stats_note_udp_rx();
            return p->q[i].len;
        }
    }
    return -1;
}

int udp_sendto(uint16_t src_port, uint32_t dst_ip, uint16_t dst_port, const void *payload, uint16_t len)
{
    if (payload == 0 || len == 0) {
        return -1;
    }
    net_stats_note_udp_tx();
    return udp_deliver(dst_port, dst_ip, src_port, payload, len);
}

int udp_recv(uint16_t port, void *payload, uint16_t cap, uint32_t *src_ip, uint16_t *src_port)
{
    struct udp_port *p = find_port(port);
    if (!p || payload == 0) {
        return -1;
    }

    for (int i = 0; i < UDP_QUEUE_CAP; i++) {
        if (p->q[i].used) {
            uint16_t n = p->q[i].len < cap ? p->q[i].len : cap;
            for (uint16_t j = 0; j < n; j++) {
                ((uint8_t *)payload)[j] = p->q[i].payload[j];
            }
            if (src_ip) {
                *src_ip = p->q[i].src_ip;
            }
            if (src_port) {
                *src_port = p->q[i].src_port;
            }
            p->q[i].used = false;
            return n;
        }
    }
    return 0;
}

void udp_input(uint32_t src_ip, uint32_t dst_ip, const uint8_t *payload, uint16_t len)
{
    (void)dst_ip;
    if (payload == 0 || len < 8) {
        return;
    }
    uint16_t src_port = (uint16_t)((payload[0] << 8) | payload[1]);
    uint16_t dst_port = (uint16_t)((payload[2] << 8) | payload[3]);
    uint16_t udp_len = (uint16_t)((payload[4] << 8) | payload[5]);
    if (udp_len < 8 || udp_len > len) {
        return;
    }
    (void)udp_deliver(dst_port, src_ip, src_port, payload + 8, (uint16_t)(udp_len - 8));
}

#ifdef NEVERMIND_HOST_TEST
void net_test_inject_udp(uint32_t src_ip, uint16_t src_port, uint16_t dst_port, const void *payload,
                         uint16_t len)
{
    (void)udp_deliver(dst_port, src_ip, src_port, payload, len);
}
#endif
