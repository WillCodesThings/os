#pragma once
#include <stdint.h>
#include <stddef.h>

// Ethernet
#define ETH_HEADER_SIZE 14
#define ETH_TYPE_IPV4   0x0800
#define ETH_TYPE_ARP    0x0806

// IP
#define IP_HEADER_SIZE  20
#define IP_PROTO_ICMP   1
#define IP_PROTO_TCP    6
#define IP_PROTO_UDP    17

// ICMP
#define ICMP_ECHO_REPLY   0
#define ICMP_ECHO_REQUEST 8

// ARP
#define ARP_HARDWARE_ETHERNET 1
#define ARP_OPERATION_REQUEST 1
#define ARP_OPERATION_REPLY   2

// Ethernet header
typedef struct {
    uint8_t dest[6];
    uint8_t src[6];
    uint16_t type;     // Big-endian
} __attribute__((packed)) eth_header_t;

// IPv4 header
typedef struct {
    uint8_t version_ihl;   // Version (4 bits) + IHL (4 bits)
    uint8_t tos;           // Type of Service
    uint16_t total_length; // Big-endian
    uint16_t id;           // Big-endian
    uint16_t flags_frag;   // Flags (3 bits) + Fragment offset (13 bits)
    uint8_t ttl;           // Time to Live
    uint8_t protocol;      // Protocol
    uint16_t checksum;     // Header checksum
    uint32_t src_ip;       // Source IP (big-endian)
    uint32_t dest_ip;      // Destination IP (big-endian)
} __attribute__((packed)) ipv4_header_t;

// ICMP header
typedef struct {
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint16_t id;
    uint16_t sequence;
} __attribute__((packed)) icmp_header_t;

// ARP header
typedef struct {
    uint16_t hardware_type;    // Big-endian
    uint16_t protocol_type;    // Big-endian
    uint8_t hardware_size;
    uint8_t protocol_size;
    uint16_t operation;        // Big-endian
    uint8_t sender_mac[6];
    uint32_t sender_ip;        // Big-endian
    uint8_t target_mac[6];
    uint32_t target_ip;        // Big-endian
} __attribute__((packed)) arp_header_t;

// UDP header
typedef struct {
    uint16_t src_port;     // Big-endian
    uint16_t dest_port;    // Big-endian
    uint16_t length;       // Big-endian
    uint16_t checksum;     // Big-endian
} __attribute__((packed)) udp_header_t;

// TCP header
typedef struct {
    uint16_t src_port;     // Big-endian
    uint16_t dest_port;    // Big-endian
    uint32_t seq_num;      // Big-endian
    uint32_t ack_num;      // Big-endian
    uint8_t data_offset;   // Data offset (4 bits) + reserved (4 bits)
    uint8_t flags;
    uint16_t window;       // Big-endian
    uint16_t checksum;     // Big-endian
    uint16_t urgent;       // Big-endian
} __attribute__((packed)) tcp_header_t;

// TCP flags
#define TCP_FLAG_FIN 0x01
#define TCP_FLAG_SYN 0x02
#define TCP_FLAG_RST 0x04
#define TCP_FLAG_PSH 0x08
#define TCP_FLAG_ACK 0x10
#define TCP_FLAG_URG 0x20

// ARP cache entry
typedef struct {
    uint32_t ip;
    uint8_t mac[6];
    uint8_t valid;
} arp_entry_t;

// Byte order helpers (host to network, network to host)
uint16_t htons(uint16_t hostshort);
uint32_t htonl(uint32_t hostlong);
uint16_t ntohs(uint16_t netshort);
uint32_t ntohl(uint32_t netlong);

// IP address helpers
uint32_t ip_to_uint32(uint8_t a, uint8_t b, uint8_t c, uint8_t d);
void uint32_to_ip(uint32_t ip, uint8_t *out);

// Network initialization
void net_init(void);

// Configuration
void net_set_ip(uint32_t ip);
void net_set_gateway(uint32_t gateway);
void net_set_netmask(uint32_t netmask);
uint32_t net_get_ip(void);
uint32_t net_get_gateway(void);

// Packet handling
void net_process_packet(void);
int net_handle_packet(uint8_t *buffer, size_t length);

// Ethernet
int eth_send(const uint8_t *dest_mac, uint16_t type, const void *data, size_t length);

// ARP
void arp_init(void);
int arp_lookup(uint32_t ip, uint8_t *mac);
void arp_handle(const uint8_t *packet, size_t length);
int arp_request(uint32_t ip);

// IP
uint16_t ip_checksum(const void *data, size_t length);
int ip_send(uint32_t dest_ip, uint8_t protocol, const void *data, size_t length);
void ip_handle(const uint8_t *packet, size_t length);

// ICMP
void icmp_handle(const uint8_t *packet, size_t length, uint32_t src_ip);
int icmp_send_echo_request(uint32_t dest_ip, uint16_t id, uint16_t seq);
int ping(uint32_t dest_ip, int count);
