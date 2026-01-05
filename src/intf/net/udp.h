#pragma once
#include <stdint.h>
#include <stddef.h>
#include "net/net.h"

// Maximum UDP sockets
#define MAX_UDP_SOCKETS 16

// UDP receive buffer size
#define UDP_RECV_BUFFER_SIZE 2048

// UDP Socket
typedef struct {
    uint16_t local_port;
    uint32_t remote_ip;
    uint16_t remote_port;

    uint8_t recv_buffer[UDP_RECV_BUFFER_SIZE];
    size_t recv_len;
    uint32_t recv_src_ip;
    uint16_t recv_src_port;

    int used;
    int bound;
} udp_socket_t;

// UDP functions
void udp_init(void);
int udp_socket(void);
int udp_bind(int sock, uint16_t port);
int udp_connect(int sock, uint32_t dest_ip, uint16_t dest_port);
int udp_sendto(int sock, const void *data, size_t length,
               uint32_t dest_ip, uint16_t dest_port);
int udp_send(int sock, const void *data, size_t length);
int udp_recvfrom(int sock, void *buffer, size_t max_length,
                 uint32_t *src_ip, uint16_t *src_port);
int udp_recv(int sock, void *buffer, size_t max_length);
int udp_close(int sock);
void udp_handle(const uint8_t *packet, size_t length, uint32_t src_ip);
