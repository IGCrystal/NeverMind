#include <stdint.h>
#include <stdio.h>

#include "nm/socket.h"

int main(void)
{
    int s = nm_socket(NM_AF_INET, NM_SOCK_STREAM, 0);
    struct nm_sockaddr_in addr = {.sin_family = NM_AF_INET, .sin_port = 8080, .sin_addr = 0x7F000001};
    if (s < 0 || nm_connect(s, &addr) != 0) {
        puts("http_client: connect failed");
        return 1;
    }

    const char req[] = "GET / HTTP/1.1\r\nHost: nevermind\r\n\r\n";
    (void)nm_sendto(s, req, sizeof(req) - 1, 0);

    char resp[256] = {0};
    int64_t n = nm_recvfrom(s, resp, sizeof(resp) - 1, 0);
    if (n > 0) {
        resp[n] = '\0';
        puts(resp);
    }
    (void)nm_close_socket(s);
    return 0;
}
