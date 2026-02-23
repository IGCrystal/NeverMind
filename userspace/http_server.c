#include <stdint.h>
#include <stdio.h>

#include "nm/socket.h"

int main(void)
{
    int s = nm_socket(NM_AF_INET, NM_SOCK_STREAM, 0);
    struct nm_sockaddr_in addr = {.sin_family = NM_AF_INET, .sin_port = 8080, .sin_addr = 0x7F000001};
    if (s < 0 || nm_bind(s, &addr) != 0 || nm_listen(s, 4) != 0) {
        puts("http_server: setup failed");
        return 1;
    }

    int c = nm_accept(s, 0);
    if (c < 0) {
        puts("http_server: accept failed");
        return 1;
    }

    char req[256] = {0};
    (void)nm_recvfrom(c, req, sizeof(req), 0);

    const char resp[] = "HTTP/1.1 200 OK\r\nContent-Length: 12\r\n\r\nNeverMind OK";
    (void)nm_sendto(c, resp, sizeof(resp) - 1, 0);
    (void)nm_close_socket(c);
    (void)nm_close_socket(s);
    puts("http_server: served one request");
    return 0;
}
