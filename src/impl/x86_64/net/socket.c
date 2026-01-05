#include "net/socket.h"
#include "net/tcp.h"
#include "net/udp.h"
#include "net/net.h"
#include "utils/memory.h"
#include "utils/string.h"

// Socket table
static socket_t sockets[MAX_SOCKETS];

void socket_init(void) {
    memset(sockets, 0, sizeof(sockets));
    tcp_init();
    udp_init();
}

// Create socket
int socket_create(int type) {
    // Find free slot
    int sock = -1;
    for (int i = 0; i < MAX_SOCKETS; i++) {
        if (!sockets[i].used) {
            sock = i;
            break;
        }
    }

    if (sock < 0) return -1;

    sockets[sock].type = type;
    sockets[sock].used = 1;
    sockets[sock].protocol_sock = -1;

    // Allocate protocol-level socket
    if (type == SOCK_STREAM) {
        // TCP socket allocated on connect/listen
    } else if (type == SOCK_DGRAM) {
        sockets[sock].protocol_sock = udp_socket();
        if (sockets[sock].protocol_sock < 0) {
            sockets[sock].used = 0;
            return -1;
        }
    }

    return sock;
}

// Bind to port
int socket_bind(int sock, uint16_t port) {
    if (sock < 0 || sock >= MAX_SOCKETS) return -1;
    if (!sockets[sock].used) return -1;

    if (sockets[sock].type == SOCK_DGRAM) {
        return udp_bind(sockets[sock].protocol_sock, port);
    }

    // For TCP, binding happens on listen
    return 0;
}

// Listen (TCP only)
int socket_listen(int sock) {
    if (sock < 0 || sock >= MAX_SOCKETS) return -1;
    if (!sockets[sock].used) return -1;
    if (sockets[sock].type != SOCK_STREAM) return -1;

    // Would need a port to listen on - simplified
    return -1;
}

// Accept (TCP only)
int socket_accept(int sock) {
    if (sock < 0 || sock >= MAX_SOCKETS) return -1;
    if (!sockets[sock].used) return -1;
    if (sockets[sock].type != SOCK_STREAM) return -1;

    int new_tcp = tcp_accept(sockets[sock].protocol_sock);
    if (new_tcp < 0) return -1;

    // Find free socket slot for accepted connection
    int new_sock = -1;
    for (int i = 0; i < MAX_SOCKETS; i++) {
        if (!sockets[i].used) {
            new_sock = i;
            break;
        }
    }

    if (new_sock < 0) {
        tcp_close(new_tcp);
        return -1;
    }

    sockets[new_sock].type = SOCK_STREAM;
    sockets[new_sock].used = 1;
    sockets[new_sock].protocol_sock = new_tcp;

    return new_sock;
}

// Connect
int socket_connect(int sock, uint32_t addr, uint16_t port) {
    if (sock < 0 || sock >= MAX_SOCKETS) return -1;
    if (!sockets[sock].used) return -1;

    if (sockets[sock].type == SOCK_STREAM) {
        int tcp_sock = tcp_connect(addr, port, 0);
        if (tcp_sock < 0) return -1;
        sockets[sock].protocol_sock = tcp_sock;
        return 0;
    } else if (sockets[sock].type == SOCK_DGRAM) {
        return udp_connect(sockets[sock].protocol_sock, addr, port);
    }

    return -1;
}

// Send
int socket_send(int sock, const void *buf, size_t len) {
    if (sock < 0 || sock >= MAX_SOCKETS) return -1;
    if (!sockets[sock].used) return -1;

    if (sockets[sock].type == SOCK_STREAM) {
        return tcp_send(sockets[sock].protocol_sock, buf, len);
    } else if (sockets[sock].type == SOCK_DGRAM) {
        return udp_send(sockets[sock].protocol_sock, buf, len);
    }

    return -1;
}

// Receive
int socket_recv(int sock, void *buf, size_t len) {
    if (sock < 0 || sock >= MAX_SOCKETS) return -1;
    if (!sockets[sock].used) return -1;

    if (sockets[sock].type == SOCK_STREAM) {
        return tcp_recv(sockets[sock].protocol_sock, buf, len);
    } else if (sockets[sock].type == SOCK_DGRAM) {
        return udp_recv(sockets[sock].protocol_sock, buf, len);
    }

    return -1;
}

// Send to (UDP)
int socket_sendto(int sock, const void *buf, size_t len, uint32_t addr, uint16_t port) {
    if (sock < 0 || sock >= MAX_SOCKETS) return -1;
    if (!sockets[sock].used) return -1;
    if (sockets[sock].type != SOCK_DGRAM) return -1;

    return udp_sendto(sockets[sock].protocol_sock, buf, len, addr, port);
}

// Receive from (UDP)
int socket_recvfrom(int sock, void *buf, size_t len, uint32_t *addr, uint16_t *port) {
    if (sock < 0 || sock >= MAX_SOCKETS) return -1;
    if (!sockets[sock].used) return -1;
    if (sockets[sock].type != SOCK_DGRAM) return -1;

    return udp_recvfrom(sockets[sock].protocol_sock, buf, len, addr, port);
}

// Close
int socket_close(int sock) {
    if (sock < 0 || sock >= MAX_SOCKETS) return -1;
    if (!sockets[sock].used) return -1;

    if (sockets[sock].type == SOCK_STREAM) {
        tcp_close(sockets[sock].protocol_sock);
    } else if (sockets[sock].type == SOCK_DGRAM) {
        udp_close(sockets[sock].protocol_sock);
    }

    sockets[sock].used = 0;
    return 0;
}

// Parse IP address from string (e.g., "10.0.2.2") - returns network byte order
static uint32_t parse_ip_addr(const char *str) {
    uint32_t octets[4] = {0, 0, 0, 0};
    int octet_idx = 0;

    while (*str && octet_idx < 4) {
        if (*str >= '0' && *str <= '9') {
            octets[octet_idx] = octets[octet_idx] * 10 + (*str - '0');
        } else if (*str == '.') {
            octet_idx++;
        }
        str++;
    }

    // Return in network byte order (same as ip_to_uint32)
    uint32_t host_order = (octets[0] << 24) | (octets[1] << 16) | (octets[2] << 8) | octets[3];
    return htonl(host_order);
}

// Simple HTTP GET request
int http_get(const char *host, uint16_t port, const char *path,
             char *response, size_t max_response) {
    // Create socket
    int sock = socket_create(SOCK_STREAM);
    if (sock < 0) return -1;

    // Parse host as IP address
    uint32_t ip = parse_ip_addr(host);
    if (ip == 0) {
        // Fallback to gateway if parsing failed
        ip = net_get_gateway();
    }

    if (socket_connect(sock, ip, port) < 0) {
        socket_close(sock);
        return -1;
    }

    // Build HTTP request
    char request[512];
    int req_len = 0;

    // Simple request building
    const char *method = "GET ";
    for (int i = 0; method[i]; i++) request[req_len++] = method[i];
    for (int i = 0; path[i]; i++) request[req_len++] = path[i];

    const char *http_ver = " HTTP/1.0\r\nHost: ";
    for (int i = 0; http_ver[i]; i++) request[req_len++] = http_ver[i];
    for (int i = 0; host[i]; i++) request[req_len++] = host[i];

    const char *end = "\r\nConnection: close\r\n\r\n";
    for (int i = 0; end[i]; i++) request[req_len++] = end[i];

    // Send request
    if (socket_send(sock, request, req_len) < 0) {
        socket_close(sock);
        return -1;
    }

    // Receive response
    size_t total = 0;
    while (total < max_response - 1) {
        int n = socket_recv(sock, response + total, max_response - total - 1);
        if (n <= 0) break;
        total += n;
    }

    response[total] = 0;

    socket_close(sock);
    return total;
}
