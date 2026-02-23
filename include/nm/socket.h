#ifndef NM_SOCKET_H
#define NM_SOCKET_H

#include <stdint.h>

#define NM_AF_INET 2

#define NM_SOCK_STREAM 1
#define NM_SOCK_DGRAM 2

struct nm_sockaddr_in {
    uint16_t sin_family;
    uint16_t sin_port;
    uint32_t sin_addr;
};

int nm_socket(int domain, int type, int protocol);
int nm_bind(int sockfd, const struct nm_sockaddr_in *addr);
int nm_listen(int sockfd, int backlog);
int nm_accept(int sockfd, struct nm_sockaddr_in *addr);
int nm_connect(int sockfd, const struct nm_sockaddr_in *addr);
int64_t nm_sendto(int sockfd, const void *buf, uint64_t len, const struct nm_sockaddr_in *addr);
int64_t nm_recvfrom(int sockfd, void *buf, uint64_t len, struct nm_sockaddr_in *addr);
int nm_close_socket(int sockfd);

#endif
