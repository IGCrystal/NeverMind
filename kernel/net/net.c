#include "nm/net.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "nm/errno.h"
#include "nm/rtl8139.h"

void ipv4_input(const uint8_t *packet, uint16_t len);
void arp_input(const uint8_t *packet, uint16_t len);

static struct {
    uint8_t mac[6];
    uint32_t ip;
    uint32_t gw;
    uint32_t mask;
    bool loopback;
} net_cfg;

static struct nm_net_stats stats;
static volatile uint32_t net_lock_word;

static inline void net_lock(void)
{
    while (__sync_lock_test_and_set(&net_lock_word, 1U) != 0U) {
        __asm__ volatile("pause");
    }
}

static inline void net_unlock(void)
{
    __sync_lock_release(&net_lock_word);
}

static void copy_bytes(uint8_t *dst, const uint8_t *src, uint64_t len)
{
    for (uint64_t i = 0; i < len; i++) {
        dst[i] = src[i];
    }
}

struct __attribute__((packed)) eth_hdr {
    uint8_t dst[6];
    uint8_t src[6];
    uint16_t eth_type;
};

static uint16_t bswap16(uint16_t v)
{
    return (uint16_t)((v >> 8) | (v << 8));
}

void net_init(void)
{
    net_lock_word = 0;
    net_lock();
    for (int i = 0; i < 6; i++) {
        net_cfg.mac[i] = (uint8_t)(0x52 + i);
    }
    net_cfg.ip = 0xC0A80164;
    net_cfg.gw = 0xC0A80101;
    net_cfg.mask = 0xFFFFFF00;
    net_cfg.loopback = true;

    stats = (struct nm_net_stats){0};
    net_unlock();
}

void net_set_identity(const uint8_t mac[6], uint32_t ip, uint32_t gateway, uint32_t mask)
{
    net_lock();
    if (mac) {
        for (int i = 0; i < 6; i++) {
            net_cfg.mac[i] = mac[i];
        }
    }
    net_cfg.ip = ip;
    net_cfg.gw = gateway;
    net_cfg.mask = mask;
    net_unlock();
}

int net_send_frame(const void *frame, uint64_t len)
{
    if (frame == 0 || len < sizeof(struct eth_hdr)) {
        return NM_ERR(NM_EFAIL);
    }
    net_lock();
    stats.tx_frames++;
    net_unlock();

#ifdef NEVERMIND_HOST_TEST
    bool loopback = net_cfg.loopback;
    if (loopback) {
        return net_input_frame(frame, len);
    }
    return (int)len;
#else
    return (int)rtl8139_send(frame, len);
#endif
}

int net_input_frame(const void *frame, uint64_t len)
{
    if (frame == 0 || len < sizeof(struct eth_hdr)) {
        net_lock();
        stats.rx_dropped++;
        net_unlock();
        return NM_ERR(NM_EFAIL);
    }

    const struct eth_hdr *eth = (const struct eth_hdr *)frame;
    uint16_t eth_type = bswap16(eth->eth_type);

    net_lock();
    stats.rx_frames++;
    net_unlock();
    const uint8_t *payload = (const uint8_t *)frame + sizeof(*eth);
    uint16_t payload_len = (uint16_t)(len - sizeof(*eth));

    if (eth_type == NM_ETH_TYPE_ARP) {
        arp_input(payload, payload_len);
        return 0;
    }
    if (eth_type == NM_ETH_TYPE_IPV4) {
        ipv4_input(payload, payload_len);
        return 0;
    }

    net_lock();
    stats.rx_dropped++;
    net_unlock();
    return NM_ERR(NM_EFAIL);
}

void net_poll(void)
{
#ifndef NEVERMIND_HOST_TEST
    uint8_t frame[2048];
    int64_t n = rtl8139_recv(frame, sizeof(frame));
    if (n > 0) {
        (void)net_input_frame(frame, (uint64_t)n);
    }
#endif
}

struct nm_net_stats net_get_stats(void)
{
    net_lock();
    struct nm_net_stats out = stats;
    net_unlock();
    return out;
}

#ifdef NEVERMIND_HOST_TEST
void net_test_set_loopback(bool on)
{
    net_lock();
    net_cfg.loopback = on;
    net_unlock();
}
#endif

void net_stats_note_arp_hit(void)
{
    net_lock();
    stats.arp_hits++;
    net_unlock();
}

void net_stats_note_arp_miss(void)
{
    net_lock();
    stats.arp_misses++;
    net_unlock();
}

void net_stats_note_icmp_req(void)
{
    net_lock();
    stats.icmp_echo_req++;
    net_unlock();
}

void net_stats_note_icmp_rep(void)
{
    net_lock();
    stats.icmp_echo_rep++;
    net_unlock();
}

void net_stats_note_udp_rx(void)
{
    net_lock();
    stats.udp_rx++;
    net_unlock();
}

void net_stats_note_udp_tx(void)
{
    net_lock();
    stats.udp_tx++;
    net_unlock();
}

void net_stats_note_tcp_conn(void)
{
    net_lock();
    stats.tcp_conn++;
    net_unlock();
}

uint32_t net_local_ip(void)
{
    net_lock();
    uint32_t ip = net_cfg.ip;
    net_unlock();
    return ip;
}

void net_local_mac(uint8_t out[6])
{
    net_lock();
    copy_bytes(out, net_cfg.mac, 6);
    net_unlock();
}
