#pragma once
#include <stdint.h>
#include <stddef.h>

// Socket types
#define SOCK_STREAM 1  // TCP
#define SOCK_DGRAM  2  // UDP

// Address families
#define AF_INET 2

// Socket address structure
typedef struct {
    uint16_t family;
    uint16_t port;      // Network byte order
    uint32_t addr;      // Network byte order
    uint8_t zero[8];
} sockaddr_in_t;

// Maximum sockets
#define MAX_SOCKETS 32

// Socket structure
typedef struct {
    int type;           // SOCK_STREAM or SOCK_DGRAM
    int protocol_sock;  // Index into TCP or UDP socket array
    int used;
} socket_t;

// Socket API
int socket_create(int type);
int socket_bind(int sock, uint16_t port);
int socket_listen(int sock);
int socket_accept(int sock);
int socket_connect(int sock, uint32_t addr, uint16_t port);
int socket_send(int sock, const void *buf, size_t len);
int socket_recv(int sock, void *buf, size_t len);
int socket_sendto(int sock, const void *buf, size_t len, uint32_t addr, uint16_t port);
int socket_recvfrom(int sock, void *buf, size_t len, uint32_t *addr, uint16_t *port);
int socket_close(int sock);

// Initialize socket subsystem
void socket_init(void);

// HTTP helpers
int http_get(const char *host, uint16_t port, const char *path,
             char *response, size_t max_response);
