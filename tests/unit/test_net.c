#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "nm/net.h"
#include "nm/socket.h"

static void test_arp_cache(void)
{
    uint8_t mac[6] = {0x10, 0x22, 0x33, 0x44, 0x55, 0x66};
    uint8_t out[6] = {0};
    uint32_t ip = 0xC0A80101;

    arp_cache_add(ip, mac);
    assert(arp_cache_lookup(ip, out));
    assert(out[0] == 0x10);
    assert(out[5] == 0x66);
}

static void test_udp_socket(void)
{
    int s1 = nm_socket(NM_AF_INET, NM_SOCK_DGRAM, 0);
    int s2 = nm_socket(NM_AF_INET, NM_SOCK_DGRAM, 0);
    assert(s1 >= 0 && s2 >= 0);

    struct nm_sockaddr_in a1 = {.sin_family = NM_AF_INET, .sin_port = 7777, .sin_addr = 0x7F000001};
    struct nm_sockaddr_in a2 = {.sin_family = NM_AF_INET, .sin_port = 8888, .sin_addr = 0x7F000001};
    assert(nm_bind(s1, &a1) == 0);
    assert(nm_bind(s2, &a2) == 0);

    const char msg[] = "udp-ok";
    assert(nm_sendto(s1, msg, 6, &a2) == 6);

    char buf[16] = {0};
    struct nm_sockaddr_in src;
    int64_t n = nm_recvfrom(s2, buf, sizeof(buf), &src);
    assert(n == 6);
    assert(buf[0] == 'u' && buf[5] == 'k');
}

static void test_tcp_socket(void)
{
    int srv = nm_socket(NM_AF_INET, NM_SOCK_STREAM, 0);
    int cli = nm_socket(NM_AF_INET, NM_SOCK_STREAM, 0);
    assert(srv >= 0 && cli >= 0);

    struct nm_sockaddr_in saddr = {.sin_family = NM_AF_INET, .sin_port = 9090, .sin_addr = 0x7F000001};
    assert(nm_bind(srv, &saddr) == 0);
    assert(nm_listen(srv, 8) == 0);

    assert(nm_connect(cli, &saddr) == 0);
    int accepted = nm_accept(srv, 0);
    assert(accepted >= 0);

    const char req[] = "GET /";
    assert(nm_sendto(cli, req, 5, 0) == 5);

    char rx[16] = {0};
    assert(nm_recvfrom(accepted, rx, sizeof(rx), 0) == 5);
    assert(rx[0] == 'G' && rx[4] == '/');
}

int main(void)
{
    net_init();
    net_test_set_loopback(true);
    test_arp_cache();
    test_udp_socket();
    test_tcp_socket();
    puts("test_net: PASS");
    return 0;
}
