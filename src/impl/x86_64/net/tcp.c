#include "net/tcp.h"
#include "net/net.h"
#include "memory/heap.h"
#include "utils/memory.h"

// TCP connections table
static tcp_connection_t tcp_connections[MAX_TCP_CONNECTIONS];
static uint16_t next_ephemeral_port = 49152;

// Pseudo-header for TCP checksum
typedef struct {
    uint32_t src_ip;
    uint32_t dest_ip;
    uint8_t zero;
    uint8_t protocol;
    uint16_t tcp_length;
} __attribute__((packed)) tcp_pseudo_header_t;

void tcp_init(void) {
    memset(tcp_connections, 0, sizeof(tcp_connections));
}

// Calculate TCP checksum
static uint16_t tcp_checksum(uint32_t src_ip, uint32_t dest_ip,
                             const void *tcp_data, size_t tcp_length) {
    tcp_pseudo_header_t pseudo;
    pseudo.src_ip = src_ip;
    pseudo.dest_ip = dest_ip;
    pseudo.zero = 0;
    pseudo.protocol = IP_PROTO_TCP;
    pseudo.tcp_length = htons(tcp_length);

    // Calculate sum over pseudo header
    uint32_t sum = 0;
    const uint16_t *ptr = (const uint16_t *)&pseudo;
    for (size_t i = 0; i < sizeof(pseudo) / 2; i++) {
        sum += ptr[i];
    }

    // Add TCP segment
    ptr = (const uint16_t *)tcp_data;
    size_t len = tcp_length;
    while (len > 1) {
        sum += *ptr++;
        len -= 2;
    }
    if (len > 0) {
        sum += *(const uint8_t *)ptr;
    }

    // Fold and complement
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return ~sum;
}

// Allocate a connection slot
static int tcp_alloc_connection(void) {
    for (int i = 0; i < MAX_TCP_CONNECTIONS; i++) {
        if (!tcp_connections[i].used) {
            memset(&tcp_connections[i], 0, sizeof(tcp_connection_t));
            tcp_connections[i].used = 1;
            return i;
        }
    }
    return -1;
}

// Find connection by local/remote address
static int tcp_find_connection(uint32_t local_ip, uint16_t local_port,
                                uint32_t remote_ip, uint16_t remote_port) {
    for (int i = 0; i < MAX_TCP_CONNECTIONS; i++) {
        if (tcp_connections[i].used &&
            tcp_connections[i].local_port == local_port &&
            (tcp_connections[i].remote_port == remote_port || tcp_connections[i].state == TCP_STATE_LISTEN) &&
            (tcp_connections[i].remote_ip == remote_ip || tcp_connections[i].state == TCP_STATE_LISTEN)) {
            return i;
        }
    }
    return -1;
}

// Send TCP segment
static int tcp_send_segment(tcp_connection_t *conn, uint8_t flags,
                            const void *data, size_t data_len) {
    size_t tcp_len = sizeof(tcp_header_t) + data_len;
    uint8_t packet[tcp_len];

    tcp_header_t *tcp = (tcp_header_t *)packet;
    tcp->src_port = htons(conn->local_port);
    tcp->dest_port = htons(conn->remote_port);
    tcp->seq_num = htonl(conn->seq_num);
    tcp->ack_num = htonl(conn->ack_num);
    tcp->data_offset = (sizeof(tcp_header_t) / 4) << 4;
    tcp->flags = flags;
    tcp->window = htons(TCP_RECV_BUFFER_SIZE);
    tcp->checksum = 0;
    tcp->urgent = 0;

    if (data && data_len > 0) {
        memcpy(packet + sizeof(tcp_header_t), data, data_len);
    }

    tcp->checksum = tcp_checksum(conn->local_ip, conn->remote_ip, packet, tcp_len);

    return ip_send(conn->remote_ip, IP_PROTO_TCP, packet, tcp_len);
}

// Connect to remote host
int tcp_connect(uint32_t dest_ip, uint16_t dest_port, uint16_t src_port) {
    int sock = tcp_alloc_connection();
    if (sock < 0) return -1;

    tcp_connection_t *conn = &tcp_connections[sock];

    conn->local_ip = net_get_ip();
    conn->local_port = src_port ? src_port : next_ephemeral_port++;
    conn->remote_ip = dest_ip;
    conn->remote_port = dest_port;
    conn->seq_num = 1000;  // Should be random
    conn->ack_num = 0;
    conn->state = TCP_STATE_SYN_SENT;

    // Send SYN
    tcp_send_segment(conn, TCP_FLAG_SYN, NULL, 0);
    conn->seq_num++;

    // Wait for SYN-ACK
    for (int i = 0; i < 5000 && conn->state == TCP_STATE_SYN_SENT; i++) {
        net_process_packet();
        for (volatile int j = 0; j < 1000; j++);
    }

    if (conn->state != TCP_STATE_ESTABLISHED) {
        conn->used = 0;
        return -1;
    }

    return sock;
}

// Listen on port
int tcp_listen(uint16_t port) {
    int sock = tcp_alloc_connection();
    if (sock < 0) return -1;

    tcp_connection_t *conn = &tcp_connections[sock];
    conn->local_ip = net_get_ip();
    conn->local_port = port;
    conn->state = TCP_STATE_LISTEN;

    return sock;
}

// Accept connection (blocking)
int tcp_accept(int listen_sock) {
    if (listen_sock < 0 || listen_sock >= MAX_TCP_CONNECTIONS) return -1;

    tcp_connection_t *listen_conn = &tcp_connections[listen_sock];
    if (listen_conn->state != TCP_STATE_LISTEN) return -1;

    // Wait for incoming connection
    while (1) {
        net_process_packet();

        // Check if we got a connection
        for (int i = 0; i < MAX_TCP_CONNECTIONS; i++) {
            if (i != listen_sock &&
                tcp_connections[i].used &&
                tcp_connections[i].local_port == listen_conn->local_port &&
                tcp_connections[i].state == TCP_STATE_ESTABLISHED) {
                return i;
            }
        }
    }
}

// Send data
int tcp_send(int sock, const void *data, size_t length) {
    if (sock < 0 || sock >= MAX_TCP_CONNECTIONS) return -1;

    tcp_connection_t *conn = &tcp_connections[sock];
    if (conn->state != TCP_STATE_ESTABLISHED) return -1;

    // Send data with PSH and ACK flags
    int ret = tcp_send_segment(conn, TCP_FLAG_PSH | TCP_FLAG_ACK, data, length);
    if (ret > 0) {
        conn->seq_num += length;
    }

    return ret > 0 ? length : -1;
}

// Receive data
int tcp_recv(int sock, void *buffer, size_t max_length) {
    if (sock < 0 || sock >= MAX_TCP_CONNECTIONS) return -1;

    tcp_connection_t *conn = &tcp_connections[sock];
    if (conn->state != TCP_STATE_ESTABLISHED &&
        conn->state != TCP_STATE_CLOSE_WAIT) return -1;

    // Poll for data
    while (conn->recv_len == 0 && conn->state == TCP_STATE_ESTABLISHED) {
        net_process_packet();
    }

    size_t to_copy = conn->recv_len;
    if (to_copy > max_length) to_copy = max_length;

    if (to_copy > 0) {
        memcpy(buffer, conn->recv_buffer, to_copy);

        // Shift remaining data
        if (to_copy < conn->recv_len) {
            memmove(conn->recv_buffer, conn->recv_buffer + to_copy,
                    conn->recv_len - to_copy);
        }
        conn->recv_len -= to_copy;
    }

    return to_copy;
}

// Close connection
int tcp_close(int sock) {
    if (sock < 0 || sock >= MAX_TCP_CONNECTIONS) return -1;

    tcp_connection_t *conn = &tcp_connections[sock];

    if (conn->state == TCP_STATE_ESTABLISHED) {
        // Send FIN
        tcp_send_segment(conn, TCP_FLAG_FIN | TCP_FLAG_ACK, NULL, 0);
        conn->seq_num++;
        conn->state = TCP_STATE_FIN_WAIT_1;

        // Wait for FIN-ACK
        for (int i = 0; i < 1000 && conn->state != TCP_STATE_CLOSED; i++) {
            net_process_packet();
            for (volatile int j = 0; j < 1000; j++);
        }
    }

    conn->used = 0;
    conn->state = TCP_STATE_CLOSED;

    return 0;
}

// Handle incoming TCP segment
void tcp_handle(const uint8_t *packet, size_t length, uint32_t src_ip) {
    if (length < sizeof(tcp_header_t)) return;

    const tcp_header_t *tcp = (const tcp_header_t *)packet;

    uint16_t src_port = ntohs(tcp->src_port);
    uint16_t dest_port = ntohs(tcp->dest_port);
    uint32_t seq = ntohl(tcp->seq_num);
    uint32_t ack = ntohl(tcp->ack_num);
    uint8_t flags = tcp->flags;

    size_t header_len = ((tcp->data_offset >> 4) & 0x0F) * 4;
    size_t data_len = length - header_len;
    const uint8_t *data = packet + header_len;

    // Find matching connection
    int sock = tcp_find_connection(net_get_ip(), dest_port, src_ip, src_port);

    if (sock < 0) {
        // Check for listening socket
        sock = tcp_find_connection(net_get_ip(), dest_port, 0, 0);

        if (sock >= 0 && tcp_connections[sock].state == TCP_STATE_LISTEN &&
            (flags & TCP_FLAG_SYN)) {
            // New incoming connection
            int new_sock = tcp_alloc_connection();
            if (new_sock >= 0) {
                tcp_connection_t *conn = &tcp_connections[new_sock];
                conn->local_ip = net_get_ip();
                conn->local_port = dest_port;
                conn->remote_ip = src_ip;
                conn->remote_port = src_port;
                conn->seq_num = 2000;
                conn->ack_num = seq + 1;
                conn->state = TCP_STATE_SYN_RECEIVED;

                // Send SYN-ACK
                tcp_send_segment(conn, TCP_FLAG_SYN | TCP_FLAG_ACK, NULL, 0);
                conn->seq_num++;
            }
        }
        return;
    }

    tcp_connection_t *conn = &tcp_connections[sock];

    switch (conn->state) {
        case TCP_STATE_SYN_SENT:
            if ((flags & (TCP_FLAG_SYN | TCP_FLAG_ACK)) == (TCP_FLAG_SYN | TCP_FLAG_ACK)) {
                conn->ack_num = seq + 1;
                conn->state = TCP_STATE_ESTABLISHED;

                // Send ACK
                tcp_send_segment(conn, TCP_FLAG_ACK, NULL, 0);
            }
            break;

        case TCP_STATE_SYN_RECEIVED:
            if (flags & TCP_FLAG_ACK) {
                conn->state = TCP_STATE_ESTABLISHED;
            }
            break;

        case TCP_STATE_ESTABLISHED:
            if (flags & TCP_FLAG_FIN) {
                conn->ack_num = seq + 1;
                conn->state = TCP_STATE_CLOSE_WAIT;
                tcp_send_segment(conn, TCP_FLAG_ACK, NULL, 0);
            }
            else if (data_len > 0) {
                // Store received data
                size_t space = TCP_RECV_BUFFER_SIZE - conn->recv_len;
                size_t to_copy = data_len < space ? data_len : space;

                if (to_copy > 0) {
                    memcpy(conn->recv_buffer + conn->recv_len, data, to_copy);
                    conn->recv_len += to_copy;
                    conn->ack_num += to_copy;

                    // Send ACK
                    tcp_send_segment(conn, TCP_FLAG_ACK, NULL, 0);
                }
            }
            break;

        case TCP_STATE_FIN_WAIT_1:
            if (flags & TCP_FLAG_ACK) {
                conn->state = TCP_STATE_FIN_WAIT_2;
            }
            if (flags & TCP_FLAG_FIN) {
                conn->ack_num = seq + 1;
                tcp_send_segment(conn, TCP_FLAG_ACK, NULL, 0);
                conn->state = TCP_STATE_CLOSED;
            }
            break;

        case TCP_STATE_FIN_WAIT_2:
            if (flags & TCP_FLAG_FIN) {
                conn->ack_num = seq + 1;
                tcp_send_segment(conn, TCP_FLAG_ACK, NULL, 0);
                conn->state = TCP_STATE_CLOSED;
            }
            break;

        case TCP_STATE_CLOSE_WAIT:
            // Wait for application to close
            break;

        case TCP_STATE_LAST_ACK:
            if (flags & TCP_FLAG_ACK) {
                conn->state = TCP_STATE_CLOSED;
                conn->used = 0;
            }
            break;
    }
}

tcp_connection_t *tcp_get_connection(int sock) {
    if (sock < 0 || sock >= MAX_TCP_CONNECTIONS) return NULL;
    if (!tcp_connections[sock].used) return NULL;
    return &tcp_connections[sock];
}
