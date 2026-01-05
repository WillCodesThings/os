#include "net/udp.h"
#include "net/net.h"
#include "memory/heap.h"
#include "utils/memory.h"

// UDP sockets table
static udp_socket_t udp_sockets[MAX_UDP_SOCKETS];
static uint16_t next_udp_port = 49152;

// Pseudo-header for UDP checksum
typedef struct {
    uint32_t src_ip;
    uint32_t dest_ip;
    uint8_t zero;
    uint8_t protocol;
    uint16_t udp_length;
} __attribute__((packed)) udp_pseudo_header_t;

void udp_init(void) {
    memset(udp_sockets, 0, sizeof(udp_sockets));
}

// Calculate UDP checksum
static uint16_t udp_checksum(uint32_t src_ip, uint32_t dest_ip,
                             const void *udp_data, size_t udp_length) {
    udp_pseudo_header_t pseudo;
    pseudo.src_ip = src_ip;
    pseudo.dest_ip = dest_ip;
    pseudo.zero = 0;
    pseudo.protocol = IP_PROTO_UDP;
    pseudo.udp_length = htons(udp_length);

    uint32_t sum = 0;
    const uint16_t *ptr = (const uint16_t *)&pseudo;
    for (size_t i = 0; i < sizeof(pseudo) / 2; i++) {
        sum += ptr[i];
    }

    ptr = (const uint16_t *)udp_data;
    size_t len = udp_length;
    while (len > 1) {
        sum += *ptr++;
        len -= 2;
    }
    if (len > 0) {
        sum += *(const uint8_t *)ptr;
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return ~sum;
}

// Create UDP socket
int udp_socket(void) {
    for (int i = 0; i < MAX_UDP_SOCKETS; i++) {
        if (!udp_sockets[i].used) {
            memset(&udp_sockets[i], 0, sizeof(udp_socket_t));
            udp_sockets[i].used = 1;
            udp_sockets[i].local_port = next_udp_port++;
            return i;
        }
    }
    return -1;
}

// Bind to port
int udp_bind(int sock, uint16_t port) {
    if (sock < 0 || sock >= MAX_UDP_SOCKETS) return -1;
    if (!udp_sockets[sock].used) return -1;

    udp_sockets[sock].local_port = port;
    udp_sockets[sock].bound = 1;
    return 0;
}

// Connect to remote host
int udp_connect(int sock, uint32_t dest_ip, uint16_t dest_port) {
    if (sock < 0 || sock >= MAX_UDP_SOCKETS) return -1;
    if (!udp_sockets[sock].used) return -1;

    udp_sockets[sock].remote_ip = dest_ip;
    udp_sockets[sock].remote_port = dest_port;
    return 0;
}

// Send to specific address
int udp_sendto(int sock, const void *data, size_t length,
               uint32_t dest_ip, uint16_t dest_port) {
    if (sock < 0 || sock >= MAX_UDP_SOCKETS) return -1;
    if (!udp_sockets[sock].used) return -1;
    if (length > 1472) return -1;  // Max UDP payload in Ethernet

    udp_socket_t *s = &udp_sockets[sock];

    size_t udp_len = sizeof(udp_header_t) + length;
    uint8_t packet[udp_len];

    udp_header_t *udp = (udp_header_t *)packet;
    udp->src_port = htons(s->local_port);
    udp->dest_port = htons(dest_port);
    udp->length = htons(udp_len);
    udp->checksum = 0;

    memcpy(packet + sizeof(udp_header_t), data, length);

    udp->checksum = udp_checksum(net_get_ip(), dest_ip, packet, udp_len);
    if (udp->checksum == 0) udp->checksum = 0xFFFF;

    int ret = ip_send(dest_ip, IP_PROTO_UDP, packet, udp_len);
    return ret > 0 ? length : -1;
}

// Send (using connected address)
int udp_send(int sock, const void *data, size_t length) {
    if (sock < 0 || sock >= MAX_UDP_SOCKETS) return -1;

    udp_socket_t *s = &udp_sockets[sock];
    if (s->remote_ip == 0) return -1;

    return udp_sendto(sock, data, length, s->remote_ip, s->remote_port);
}

// Receive from any address
int udp_recvfrom(int sock, void *buffer, size_t max_length,
                 uint32_t *src_ip, uint16_t *src_port) {
    if (sock < 0 || sock >= MAX_UDP_SOCKETS) return -1;
    if (!udp_sockets[sock].used) return -1;

    udp_socket_t *s = &udp_sockets[sock];

    // Poll for data
    while (s->recv_len == 0) {
        net_process_packet();
    }

    size_t to_copy = s->recv_len;
    if (to_copy > max_length) to_copy = max_length;

    memcpy(buffer, s->recv_buffer, to_copy);

    if (src_ip) *src_ip = s->recv_src_ip;
    if (src_port) *src_port = s->recv_src_port;

    s->recv_len = 0;

    return to_copy;
}

// Receive (ignoring source)
int udp_recv(int sock, void *buffer, size_t max_length) {
    return udp_recvfrom(sock, buffer, max_length, NULL, NULL);
}

// Close socket
int udp_close(int sock) {
    if (sock < 0 || sock >= MAX_UDP_SOCKETS) return -1;

    udp_sockets[sock].used = 0;
    return 0;
}

// Handle incoming UDP datagram
void udp_handle(const uint8_t *packet, size_t length, uint32_t src_ip) {
    if (length < sizeof(udp_header_t)) return;

    const udp_header_t *udp = (const udp_header_t *)packet;

    uint16_t dest_port = ntohs(udp->dest_port);
    uint16_t src_port = ntohs(udp->src_port);
    size_t data_len = ntohs(udp->length) - sizeof(udp_header_t);

    // Find matching socket
    for (int i = 0; i < MAX_UDP_SOCKETS; i++) {
        if (udp_sockets[i].used && udp_sockets[i].local_port == dest_port) {
            udp_socket_t *s = &udp_sockets[i];

            // Store data
            size_t to_copy = data_len;
            if (to_copy > UDP_RECV_BUFFER_SIZE) {
                to_copy = UDP_RECV_BUFFER_SIZE;
            }

            memcpy(s->recv_buffer, packet + sizeof(udp_header_t), to_copy);
            s->recv_len = to_copy;
            s->recv_src_ip = src_ip;
            s->recv_src_port = src_port;

            return;
        }
    }
}
