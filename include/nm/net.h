#ifndef NM_NET_H
#define NM_NET_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define NM_ETH_TYPE_ARP 0x0806
#define NM_ETH_TYPE_IPV4 0x0800

#define NM_IP_PROTO_ICMP 1
#define NM_IP_PROTO_TCP 6
#define NM_IP_PROTO_UDP 17

struct nm_net_stats {
    uint64_t rx_frames;
    uint64_t tx_frames;
    uint64_t rx_dropped;
    uint64_t arp_hits;
    uint64_t arp_misses;
    uint64_t icmp_echo_req;
    uint64_t icmp_echo_rep;
    uint64_t udp_rx;
    uint64_t udp_tx;
    uint64_t tcp_conn;
};

void net_init(void);
void net_set_identity(const uint8_t mac[6], uint32_t ip, uint32_t gateway, uint32_t mask);
void net_poll(void);
int net_send_frame(const void *frame, uint64_t len);
int net_input_frame(const void *frame, uint64_t len);
struct nm_net_stats net_get_stats(void);

void arp_cache_add(uint32_t ip, const uint8_t mac[6]);
bool arp_cache_lookup(uint32_t ip, uint8_t mac_out[6]);

int icmp_send_echo(uint32_t dst_ip, uint16_t id, uint16_t seq, const void *payload, uint16_t len);

int udp_bind(uint16_t port);
int udp_unbind(uint16_t port);
int udp_sendto(uint16_t src_port, uint32_t dst_ip, uint16_t dst_port, const void *payload, uint16_t len);
int udp_recv(uint16_t port, void *payload, uint16_t cap, uint32_t *src_ip, uint16_t *src_port);

int tcp_listen(uint16_t port);
int tcp_connect(uint32_t dst_ip, uint16_t dst_port, uint16_t src_port);
int tcp_accept(uint16_t listen_port);
int tcp_send(int conn_id, const void *payload, uint16_t len);
int tcp_recv(int conn_id, void *payload, uint16_t cap);
int tcp_close(int conn_id);

#ifdef NEVERMIND_HOST_TEST
void net_test_set_loopback(bool on);
void net_test_inject_udp(uint32_t src_ip, uint16_t src_port, uint16_t dst_port, const void *payload,
                         uint16_t len);
#endif

#endif
