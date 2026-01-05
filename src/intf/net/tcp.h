#pragma once
#include <stdint.h>
#include <stddef.h>
#include "net/net.h"

// TCP States
#define TCP_STATE_CLOSED      0
#define TCP_STATE_LISTEN      1
#define TCP_STATE_SYN_SENT    2
#define TCP_STATE_SYN_RECEIVED 3
#define TCP_STATE_ESTABLISHED 4
#define TCP_STATE_FIN_WAIT_1  5
#define TCP_STATE_FIN_WAIT_2  6
#define TCP_STATE_CLOSE_WAIT  7
#define TCP_STATE_CLOSING     8
#define TCP_STATE_LAST_ACK    9
#define TCP_STATE_TIME_WAIT   10

// Maximum TCP connections
#define MAX_TCP_CONNECTIONS 16

// TCP receive buffer size
#define TCP_RECV_BUFFER_SIZE 4096

// TCP Connection
typedef struct {
    uint8_t state;
    uint32_t local_ip;
    uint16_t local_port;
    uint32_t remote_ip;
    uint16_t remote_port;

    uint32_t seq_num;       // Our sequence number
    uint32_t ack_num;       // Their sequence number we've acknowledged

    uint8_t recv_buffer[TCP_RECV_BUFFER_SIZE];
    size_t recv_len;

    int used;
} tcp_connection_t;

// TCP functions
void tcp_init(void);
int tcp_connect(uint32_t dest_ip, uint16_t dest_port, uint16_t src_port);
int tcp_listen(uint16_t port);
int tcp_accept(int listen_sock);
int tcp_send(int sock, const void *data, size_t length);
int tcp_recv(int sock, void *buffer, size_t max_length);
int tcp_close(int sock);
void tcp_handle(const uint8_t *packet, size_t length, uint32_t src_ip);
tcp_connection_t *tcp_get_connection(int sock);
