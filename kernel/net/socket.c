#include "nm/socket.h"

#include <stdbool.h>
#include <stdint.h>

#include "nm/net.h"

#define SOCK_MAX 64

struct nm_socket_entry {
    bool used;
    int type;
    int proto;
    uint16_t local_port;
    uint32_t peer_ip;
    uint16_t peer_port;
    int tcp_conn_id;
    bool listening;
};

static struct nm_socket_entry socks[SOCK_MAX];
static uint16_t eph_port = 40000;

static struct nm_socket_entry *get_sock(int fd)
{
    if (fd < 0 || fd >= SOCK_MAX || !socks[fd].used) {
        return 0;
    }
    return &socks[fd];
}

int nm_socket(int domain, int type, int protocol)
{
    if (domain != NM_AF_INET) {
        return -1;
    }
    if (type != NM_SOCK_DGRAM && type != NM_SOCK_STREAM) {
        return -1;
    }

    for (int i = 0; i < SOCK_MAX; i++) {
        if (!socks[i].used) {
            socks[i].used = true;
            socks[i].type = type;
            socks[i].proto = protocol;
            socks[i].local_port = 0;
            socks[i].peer_ip = 0;
            socks[i].peer_port = 0;
            socks[i].tcp_conn_id = -1;
            socks[i].listening = false;
            return i;
        }
    }
    return -1;
}

int nm_bind(int sockfd, const struct nm_sockaddr_in *addr)
{
    struct nm_socket_entry *s = get_sock(sockfd);
    if (!s || !addr || addr->sin_family != NM_AF_INET) {
        return -1;
    }
    s->local_port = addr->sin_port;
    if (s->type == NM_SOCK_DGRAM) {
        return udp_bind(s->local_port);
    }
    return 0;
}

int nm_listen(int sockfd, int backlog)
{
    (void)backlog;
    struct nm_socket_entry *s = get_sock(sockfd);
    if (!s || s->type != NM_SOCK_STREAM || s->local_port == 0) {
        return -1;
    }
    int id = tcp_listen(s->local_port);
    if (id < 0) {
        return -1;
    }
    s->tcp_conn_id = id;
    s->listening = true;
    return 0;
}

int nm_accept(int sockfd, struct nm_sockaddr_in *addr)
{
    struct nm_socket_entry *s = get_sock(sockfd);
    if (!s || !s->listening) {
        return -1;
    }

    int conn = tcp_accept(s->local_port);
    if (conn < 0) {
        return -1;
    }

    int child = nm_socket(NM_AF_INET, NM_SOCK_STREAM, 0);
    if (child < 0) {
        return -1;
    }
    socks[child].local_port = s->local_port;
    socks[child].tcp_conn_id = conn;

    if (addr) {
        addr->sin_family = NM_AF_INET;
        addr->sin_port = s->local_port;
        addr->sin_addr = 0;
    }
    return child;
}

int nm_connect(int sockfd, const struct nm_sockaddr_in *addr)
{
    struct nm_socket_entry *s = get_sock(sockfd);
    if (!s || !addr || s->type != NM_SOCK_STREAM) {
        return -1;
    }

    if (s->local_port == 0) {
        s->local_port = eph_port++;
    }

    int conn = tcp_connect(addr->sin_addr, addr->sin_port, s->local_port);
    if (conn < 0) {
        return -1;
    }
    s->peer_ip = addr->sin_addr;
    s->peer_port = addr->sin_port;
    s->tcp_conn_id = conn;
    return 0;
}

int64_t nm_sendto(int sockfd, const void *buf, uint64_t len, const struct nm_sockaddr_in *addr)
{
    struct nm_socket_entry *s = get_sock(sockfd);
    if (!s || !buf || len == 0) {
        return -1;
    }

    if (s->type == NM_SOCK_DGRAM) {
        if (!addr) {
            return -1;
        }
        if (s->local_port == 0) {
            s->local_port = eph_port++;
            (void)udp_bind(s->local_port);
        }
        return udp_sendto(s->local_port, addr->sin_addr, addr->sin_port, buf, (uint16_t)len);
    }

    return tcp_send(s->tcp_conn_id, buf, (uint16_t)len);
}

int64_t nm_recvfrom(int sockfd, void *buf, uint64_t len, struct nm_sockaddr_in *addr)
{
    struct nm_socket_entry *s = get_sock(sockfd);
    if (!s || !buf || len == 0) {
        return -1;
    }

    if (s->type == NM_SOCK_DGRAM) {
        uint32_t src_ip = 0;
        uint16_t src_port = 0;
        int n = udp_recv(s->local_port, buf, (uint16_t)len, &src_ip, &src_port);
        if (n >= 0 && addr) {
            addr->sin_family = NM_AF_INET;
            addr->sin_addr = src_ip;
            addr->sin_port = src_port;
        }
        return n;
    }

    return tcp_recv(s->tcp_conn_id, buf, (uint16_t)len);
}

int nm_close_socket(int sockfd)
{
    struct nm_socket_entry *s = get_sock(sockfd);
    if (!s) {
        return -1;
    }

    if (s->type == NM_SOCK_DGRAM && s->local_port != 0) {
        (void)udp_unbind(s->local_port);
    }
    if (s->type == NM_SOCK_STREAM && s->tcp_conn_id > 0) {
        (void)tcp_close(s->tcp_conn_id);
    }

    s->used = false;
    return 0;
}
