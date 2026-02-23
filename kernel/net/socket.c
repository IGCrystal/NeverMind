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
static volatile uint32_t sock_lock_word;

static inline void sock_lock(void)
{
    while (__sync_lock_test_and_set(&sock_lock_word, 1U) != 0U) {
        __asm__ volatile("pause");
    }
}

static inline void sock_unlock(void)
{
    __sync_lock_release(&sock_lock_word);
}

static struct nm_socket_entry *get_sock(int fd)
{
    if (fd < 0 || fd >= SOCK_MAX || !socks[fd].used) {
        return 0;
    }
    return &socks[fd];
}

static int alloc_socket_unlocked(int type, int protocol)
{
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

int nm_socket(int domain, int type, int protocol)
{
    if (domain != NM_AF_INET) {
        return -1;
    }
    if (type != NM_SOCK_DGRAM && type != NM_SOCK_STREAM) {
        return -1;
    }

    sock_lock();
    int fd = alloc_socket_unlocked(type, protocol);
    sock_unlock();
    return fd;
}

int nm_bind(int sockfd, const struct nm_sockaddr_in *addr)
{
    sock_lock();
    struct nm_socket_entry *s = get_sock(sockfd);
    if (!s || !addr || addr->sin_family != NM_AF_INET) {
        sock_unlock();
        return -1;
    }
    uint16_t old_port = s->local_port;
    s->local_port = addr->sin_port;
    int type = s->type;
    uint16_t port = s->local_port;
    sock_unlock();
    if (type == NM_SOCK_DGRAM) {
        if (udp_bind(port) != 0) {
            sock_lock();
            s = get_sock(sockfd);
            if (s) {
                s->local_port = old_port;
            }
            sock_unlock();
            return -1;
        }
    }
    return 0;
}

int nm_listen(int sockfd, int backlog)
{
    (void)backlog;
    sock_lock();
    struct nm_socket_entry *s = get_sock(sockfd);
    if (!s || s->type != NM_SOCK_STREAM || s->local_port == 0) {
        sock_unlock();
        return -1;
    }
    uint16_t port = s->local_port;
    sock_unlock();

    int id = tcp_listen(port);
    if (id < 0) {
        return -1;
    }

    sock_lock();
    s = get_sock(sockfd);
    if (!s) {
        sock_unlock();
        (void)tcp_close(id);
        return -1;
    }
    s->tcp_conn_id = id;
    s->listening = true;
    sock_unlock();
    return 0;
}

int nm_accept(int sockfd, struct nm_sockaddr_in *addr)
{
    sock_lock();
    struct nm_socket_entry *s = get_sock(sockfd);
    if (!s || !s->listening) {
        sock_unlock();
        return -1;
    }
    uint16_t port = s->local_port;
    sock_unlock();

    int conn = tcp_accept(port);
    if (conn < 0) {
        return -1;
    }

    sock_lock();
    int child = alloc_socket_unlocked(NM_SOCK_STREAM, 0);
    if (child < 0) {
        sock_unlock();
        (void)tcp_close(conn);
        return -1;
    }
    socks[child].local_port = port;
    socks[child].tcp_conn_id = conn;
    sock_unlock();

    if (addr) {
        addr->sin_family = NM_AF_INET;
        addr->sin_port = port;
        addr->sin_addr = 0;
    }
    return child;
}

int nm_connect(int sockfd, const struct nm_sockaddr_in *addr)
{
    sock_lock();
    struct nm_socket_entry *s = get_sock(sockfd);
    if (!s || !addr || s->type != NM_SOCK_STREAM) {
        sock_unlock();
        return -1;
    }

    uint16_t local_port = s->local_port;
    bool local_port_was_auto = false;
    if (local_port == 0) {
        local_port = eph_port++;
        s->local_port = local_port;
        local_port_was_auto = true;
    }
    uint32_t dst_ip = addr->sin_addr;
    uint16_t dst_port = addr->sin_port;
    sock_unlock();

    int conn = tcp_connect(dst_ip, dst_port, local_port);
    if (conn < 0) {
        if (local_port_was_auto) {
            sock_lock();
            s = get_sock(sockfd);
            if (s && s->local_port == local_port) {
                s->local_port = 0;
            }
            sock_unlock();
        }
        return -1;
    }

    sock_lock();
    s = get_sock(sockfd);
    if (!s) {
        sock_unlock();
        (void)tcp_close(conn);
        return -1;
    }
    s->peer_ip = dst_ip;
    s->peer_port = dst_port;
    s->tcp_conn_id = conn;
    sock_unlock();
    return 0;
}

int64_t nm_sendto(int sockfd, const void *buf, uint64_t len, const struct nm_sockaddr_in *addr)
{
    sock_lock();
    struct nm_socket_entry *s = get_sock(sockfd);
    if (!s || !buf || len == 0) {
        sock_unlock();
        return -1;
    }

    if (s->type == NM_SOCK_DGRAM) {
        if (!addr) {
            sock_unlock();
            return -1;
        }
        if (s->local_port == 0) {
            s->local_port = eph_port++;
        }
        uint16_t src_port = s->local_port;
        uint32_t dst_ip = addr->sin_addr;
        uint16_t dst_port = addr->sin_port;
        sock_unlock();
        if (udp_bind(src_port) != 0) {
            return -1;
        }
        return udp_sendto(src_port, dst_ip, dst_port, buf, (uint16_t)len);
    }

    int conn_id = s->tcp_conn_id;
    sock_unlock();
    if (conn_id <= 0) {
        return -1;
    }
    return tcp_send(conn_id, buf, (uint16_t)len);
}

int64_t nm_recvfrom(int sockfd, void *buf, uint64_t len, struct nm_sockaddr_in *addr)
{
    sock_lock();
    struct nm_socket_entry *s = get_sock(sockfd);
    if (!s || !buf || len == 0) {
        sock_unlock();
        return -1;
    }

    if (s->type == NM_SOCK_DGRAM) {
        uint16_t local_port = s->local_port;
        sock_unlock();
        uint32_t src_ip = 0;
        uint16_t src_port = 0;
        int n = udp_recv(local_port, buf, (uint16_t)len, &src_ip, &src_port);
        if (n >= 0 && addr) {
            addr->sin_family = NM_AF_INET;
            addr->sin_addr = src_ip;
            addr->sin_port = src_port;
        }
        return n;
    }

    int conn_id = s->tcp_conn_id;
    sock_unlock();
    if (conn_id <= 0) {
        return -1;
    }
    return tcp_recv(conn_id, buf, (uint16_t)len);
}

int nm_close_socket(int sockfd)
{
    sock_lock();
    struct nm_socket_entry *s = get_sock(sockfd);
    if (!s) {
        sock_unlock();
        return -1;
    }

    int type = s->type;
    uint16_t local_port = s->local_port;
    int conn_id = s->tcp_conn_id;

    s->used = false;
    sock_unlock();

    if (type == NM_SOCK_DGRAM && local_port != 0) {
        (void)udp_unbind(local_port);
    }
    if (type == NM_SOCK_STREAM && conn_id > 0) {
        (void)tcp_close(conn_id);
    }

    return 0;
}
