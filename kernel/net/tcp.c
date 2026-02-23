#include "nm/net.h"

#include <stdbool.h>
#include <stdint.h>

void net_stats_note_tcp_conn(void);

#define TCP_CONN_MAX 64
#define TCP_BUF_MAX 2048

enum tcp_state {
    TCP_CLOSED = 0,
    TCP_LISTEN,
    TCP_SYN_SENT,
    TCP_SYN_RECV,
    TCP_ESTABLISHED,
};

struct tcp_conn {
    bool used;
    int id;
    enum tcp_state state;
    uint32_t local_ip;
    uint16_t local_port;
    uint32_t peer_ip;
    uint16_t peer_port;
    int peer_id;
    uint16_t rx_len;
    uint8_t rx_buf[TCP_BUF_MAX];
};

static struct tcp_conn conns[TCP_CONN_MAX];
static int next_id = 1;
static volatile uint32_t tcp_lock_word;

static inline void tcp_lock(void)
{
    while (__sync_lock_test_and_set(&tcp_lock_word, 1U) != 0U) {
        __asm__ volatile("pause");
    }
}

static inline void tcp_unlock(void)
{
    __sync_lock_release(&tcp_lock_word);
}

static struct tcp_conn *find_by_id(int id)
{
    for (int i = 0; i < TCP_CONN_MAX; i++) {
        if (conns[i].used && conns[i].id == id) {
            return &conns[i];
        }
    }
    return 0;
}

static struct tcp_conn *find_listener(uint16_t port)
{
    for (int i = 0; i < TCP_CONN_MAX; i++) {
        if (conns[i].used && conns[i].state == TCP_LISTEN && conns[i].local_port == port) {
            return &conns[i];
        }
    }
    return 0;
}

static struct tcp_conn *alloc_conn(void)
{
    for (int i = 0; i < TCP_CONN_MAX; i++) {
        if (!conns[i].used) {
            conns[i].used = true;
            conns[i].id = next_id++;
            conns[i].state = TCP_CLOSED;
            conns[i].peer_id = -1;
            conns[i].rx_len = 0;
            return &conns[i];
        }
    }
    return 0;
}

int tcp_listen(uint16_t port)
{
    tcp_lock();
    struct tcp_conn *c = alloc_conn();
    if (!c) {
        tcp_unlock();
        return -1;
    }
    c->state = TCP_LISTEN;
    c->local_ip = 0;
    c->local_port = port;
    int id = c->id;
    tcp_unlock();
    return id;
}

int tcp_connect(uint32_t dst_ip, uint16_t dst_port, uint16_t src_port)
{
    tcp_lock();
    const struct tcp_conn *listener = find_listener(dst_port);
    if (!listener) {
        tcp_unlock();
        return -1;
    }

    struct tcp_conn *cli = alloc_conn();
    struct tcp_conn *srv = alloc_conn();
    if (!cli || !srv) {
        tcp_unlock();
        return -1;
    }

    cli->state = TCP_ESTABLISHED;
    cli->local_ip = 0;
    cli->peer_ip = dst_ip;
    cli->peer_port = dst_port;
    cli->local_port = src_port;

    srv->state = TCP_SYN_RECV;
    srv->local_ip = dst_ip;
    srv->local_port = dst_port;
    srv->peer_ip = dst_ip;
    srv->peer_port = src_port;

    cli->peer_id = srv->id;
    srv->peer_id = cli->id;

    net_stats_note_tcp_conn();
    int id = cli->id;
    tcp_unlock();
    return id;
}

int tcp_accept(uint16_t listen_port)
{
    tcp_lock();
    for (int i = 0; i < TCP_CONN_MAX; i++) {
        if (conns[i].used && conns[i].state == TCP_SYN_RECV && conns[i].local_port == listen_port) {
            conns[i].state = TCP_ESTABLISHED;
            int id = conns[i].id;
            tcp_unlock();
            return id;
        }
    }
    tcp_unlock();
    return -1;
}

int tcp_send(int conn_id, const void *payload, uint16_t len)
{
    tcp_lock();
    const struct tcp_conn *c = find_by_id(conn_id);
    if (!c || c->state != TCP_ESTABLISHED || payload == 0 || len == 0) {
        tcp_unlock();
        return -1;
    }

    struct tcp_conn *peer = find_by_id(c->peer_id);
    if (!peer || peer->state != TCP_ESTABLISHED) {
        tcp_unlock();
        return -1;
    }

    uint16_t n = len > TCP_BUF_MAX ? TCP_BUF_MAX : len;
    for (uint16_t i = 0; i < n; i++) {
        peer->rx_buf[i] = ((const uint8_t *)payload)[i];
    }
    peer->rx_len = n;
    tcp_unlock();
    return n;
}

int tcp_recv(int conn_id, void *payload, uint16_t cap)
{
    tcp_lock();
    struct tcp_conn *c = find_by_id(conn_id);
    if (!c || c->state != TCP_ESTABLISHED || payload == 0) {
        tcp_unlock();
        return -1;
    }
    if (c->rx_len == 0) {
        tcp_unlock();
        return 0;
    }

    uint16_t n = c->rx_len < cap ? c->rx_len : cap;
    for (uint16_t i = 0; i < n; i++) {
        ((uint8_t *)payload)[i] = c->rx_buf[i];
    }
    c->rx_len = 0;
    tcp_unlock();
    return n;
}

int tcp_close(int conn_id)
{
    tcp_lock();
    struct tcp_conn *c = find_by_id(conn_id);
    if (!c) {
        tcp_unlock();
        return -1;
    }
    c->used = false;
    tcp_unlock();
    return 0;
}

void tcp_input(uint32_t src_ip, uint32_t dst_ip, const uint8_t *payload, uint16_t len)
{
    (void)src_ip;
    (void)dst_ip;
    (void)payload;
    (void)len;
}
