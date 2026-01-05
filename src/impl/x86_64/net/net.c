#include "net/net.h"
#include "net/tcp.h"
#include "net/udp.h"
#include "drivers/e1000.h"
#include "memory/heap.h"
#include "utils/memory.h"
#include "shell/print.h"

// Network configuration
static uint32_t local_ip = 0;
static uint32_t gateway_ip = 0;
static uint32_t netmask = 0;
static uint8_t local_mac[6];

// ARP cache
#define ARP_CACHE_SIZE 32
static arp_entry_t arp_cache[ARP_CACHE_SIZE];

// Packet buffer
static uint8_t packet_buffer[2048];

// ICMP tracking
static volatile int icmp_reply_received = 0;
static uint16_t icmp_last_id = 0;
static uint16_t icmp_last_seq = 0;

// Byte order conversion
uint16_t htons(uint16_t hostshort) {
    return ((hostshort & 0xFF) << 8) | ((hostshort >> 8) & 0xFF);
}

uint32_t htonl(uint32_t hostlong) {
    return ((hostlong & 0xFF) << 24) |
           ((hostlong & 0xFF00) << 8) |
           ((hostlong >> 8) & 0xFF00) |
           ((hostlong >> 24) & 0xFF);
}

uint16_t ntohs(uint16_t netshort) {
    return htons(netshort);
}

uint32_t ntohl(uint32_t netlong) {
    return htonl(netlong);
}

// IP helpers
uint32_t ip_to_uint32(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    return htonl(((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)c << 8) | d);
}

void uint32_to_ip(uint32_t ip, uint8_t *out) {
    uint32_t host_ip = ntohl(ip);
    out[0] = (host_ip >> 24) & 0xFF;
    out[1] = (host_ip >> 16) & 0xFF;
    out[2] = (host_ip >> 8) & 0xFF;
    out[3] = host_ip & 0xFF;
}

// Network initialization
void net_init(void) {
    // Clear ARP cache
    memset(arp_cache, 0, sizeof(arp_cache));

    // Get MAC address from e1000
    e1000_get_mac_address(local_mac);

    // Default configuration (10.0.2.15 is QEMU's default)
    local_ip = ip_to_uint32(10, 0, 2, 15);
    gateway_ip = ip_to_uint32(10, 0, 2, 2);
    netmask = ip_to_uint32(255, 255, 255, 0);
}

void net_set_ip(uint32_t ip) {
    local_ip = ip;
}

void net_set_gateway(uint32_t gateway) {
    gateway_ip = gateway;
}

void net_set_netmask(uint32_t mask) {
    netmask = mask;
}

uint32_t net_get_ip(void) {
    return local_ip;
}

uint32_t net_get_gateway(void) {
    return gateway_ip;
}

// ARP initialization
void arp_init(void) {
    memset(arp_cache, 0, sizeof(arp_cache));
}

// ARP cache lookup
int arp_lookup(uint32_t ip, uint8_t *mac) {
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (arp_cache[i].valid && arp_cache[i].ip == ip) {
            memcpy(mac, arp_cache[i].mac, 6);
            return 0;
        }
    }
    return -1;  // Not found
}

// Add to ARP cache
static void arp_cache_add(uint32_t ip, const uint8_t *mac) {
    // Find empty slot or oldest entry
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (!arp_cache[i].valid) {
            arp_cache[i].ip = ip;
            memcpy(arp_cache[i].mac, mac, 6);
            arp_cache[i].valid = 1;
            return;
        }
    }

    // Overwrite first entry if full
    arp_cache[0].ip = ip;
    memcpy(arp_cache[0].mac, mac, 6);
    arp_cache[0].valid = 1;
}

// Send ARP request
int arp_request(uint32_t ip) {
    uint8_t packet[sizeof(eth_header_t) + sizeof(arp_header_t)];

    eth_header_t *eth = (eth_header_t *)packet;
    arp_header_t *arp = (arp_header_t *)(packet + sizeof(eth_header_t));

    // Broadcast destination
    memset(eth->dest, 0xFF, 6);
    memcpy(eth->src, local_mac, 6);
    eth->type = htons(ETH_TYPE_ARP);

    // ARP request
    arp->hardware_type = htons(ARP_HARDWARE_ETHERNET);
    arp->protocol_type = htons(ETH_TYPE_IPV4);
    arp->hardware_size = 6;
    arp->protocol_size = 4;
    arp->operation = htons(ARP_OPERATION_REQUEST);
    memcpy(arp->sender_mac, local_mac, 6);
    arp->sender_ip = local_ip;
    memset(arp->target_mac, 0, 6);
    arp->target_ip = ip;

    return e1000_send_packet(packet, sizeof(packet));
}

// Handle ARP packet
void arp_handle(const uint8_t *packet, size_t length) {
    if (length < sizeof(eth_header_t) + sizeof(arp_header_t)) return;

    const arp_header_t *arp = (const arp_header_t *)(packet + sizeof(eth_header_t));

    // Only handle Ethernet/IPv4 ARP
    if (ntohs(arp->hardware_type) != ARP_HARDWARE_ETHERNET) return;
    if (ntohs(arp->protocol_type) != ETH_TYPE_IPV4) return;

    // Add sender to cache
    arp_cache_add(arp->sender_ip, arp->sender_mac);

    // Handle ARP request for our IP
    if (ntohs(arp->operation) == ARP_OPERATION_REQUEST &&
        arp->target_ip == local_ip) {

        // Send ARP reply
        uint8_t reply[sizeof(eth_header_t) + sizeof(arp_header_t)];

        eth_header_t *eth = (eth_header_t *)reply;
        arp_header_t *reply_arp = (arp_header_t *)(reply + sizeof(eth_header_t));

        memcpy(eth->dest, arp->sender_mac, 6);
        memcpy(eth->src, local_mac, 6);
        eth->type = htons(ETH_TYPE_ARP);

        reply_arp->hardware_type = htons(ARP_HARDWARE_ETHERNET);
        reply_arp->protocol_type = htons(ETH_TYPE_IPV4);
        reply_arp->hardware_size = 6;
        reply_arp->protocol_size = 4;
        reply_arp->operation = htons(ARP_OPERATION_REPLY);
        memcpy(reply_arp->sender_mac, local_mac, 6);
        reply_arp->sender_ip = local_ip;
        memcpy(reply_arp->target_mac, arp->sender_mac, 6);
        reply_arp->target_ip = arp->sender_ip;

        e1000_send_packet(reply, sizeof(reply));
    }
}

// Calculate IP checksum
uint16_t ip_checksum(const void *data, size_t length) {
    const uint16_t *ptr = (const uint16_t *)data;
    uint32_t sum = 0;

    while (length > 1) {
        sum += *ptr++;
        length -= 2;
    }

    if (length > 0) {
        sum += *(const uint8_t *)ptr;
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return ~sum;
}

// Send Ethernet frame
int eth_send(const uint8_t *dest_mac, uint16_t type, const void *data, size_t length) {
    if (length > 1500) return -1;

    uint8_t packet[sizeof(eth_header_t) + 1500];

    eth_header_t *eth = (eth_header_t *)packet;
    memcpy(eth->dest, dest_mac, 6);
    memcpy(eth->src, local_mac, 6);
    eth->type = htons(type);

    memcpy(packet + sizeof(eth_header_t), data, length);

    return e1000_send_packet(packet, sizeof(eth_header_t) + length);
}

// Send IP packet
int ip_send(uint32_t dest_ip, uint8_t protocol, const void *data, size_t length) {
    uint8_t packet[sizeof(ipv4_header_t) + 1500];

    ipv4_header_t *ip = (ipv4_header_t *)packet;

    // Fill IP header
    ip->version_ihl = 0x45;  // IPv4, 5 dwords header
    ip->tos = 0;
    ip->total_length = htons(sizeof(ipv4_header_t) + length);
    ip->id = htons(0);
    ip->flags_frag = 0;
    ip->ttl = 64;
    ip->protocol = protocol;
    ip->checksum = 0;
    ip->src_ip = local_ip;
    ip->dest_ip = dest_ip;

    // Calculate checksum
    ip->checksum = ip_checksum(ip, sizeof(ipv4_header_t));

    // Copy data
    memcpy(packet + sizeof(ipv4_header_t), data, length);

    // Determine next hop
    uint32_t next_hop = dest_ip;
    if ((dest_ip & netmask) != (local_ip & netmask)) {
        next_hop = gateway_ip;
    }

    // Get MAC address
    uint8_t dest_mac[6];
    if (arp_lookup(next_hop, dest_mac) != 0) {
        // Need to do ARP
        print_str("Sending ARP request...\n");
        int arp_result = arp_request(next_hop);
        if (arp_result < 0) {
            print_str("ARP send failed with code ");
            print_int(arp_result);
            print_str("\n");
            return -1;
        }
        print_str("ARP sent (");
        print_int(arp_result);
        print_str(" bytes), waiting for reply...\n");

        // Wait for ARP reply (simple polling) - longer timeout
        for (int i = 0; i < 5000; i++) {
            net_process_packet();
            if (arp_lookup(next_hop, dest_mac) == 0) {
                print_str("ARP reply received!\n");
                break;
            }
            for (volatile int j = 0; j < 50000; j++);  // Delay
        }

        if (arp_lookup(next_hop, dest_mac) != 0) {
            print_str("ARP timeout - no reply received\n");
            return -1;  // ARP failed
        }
    }

    return eth_send(dest_mac, ETH_TYPE_IPV4, packet, sizeof(ipv4_header_t) + length);
}

// Handle IP packet
void ip_handle(const uint8_t *packet, size_t length) {
    if (length < sizeof(eth_header_t) + sizeof(ipv4_header_t)) return;

    const ipv4_header_t *ip = (const ipv4_header_t *)(packet + sizeof(eth_header_t));

    // Verify it's for us
    if (ip->dest_ip != local_ip && ip->dest_ip != 0xFFFFFFFF) return;

    // Handle based on protocol
    size_t ip_header_len = (ip->version_ihl & 0x0F) * 4;
    const uint8_t *payload = packet + sizeof(eth_header_t) + ip_header_len;
    size_t payload_len = ntohs(ip->total_length) - ip_header_len;

    switch (ip->protocol) {
        case IP_PROTO_ICMP:
            icmp_handle(payload, payload_len, ip->src_ip);
            break;

        case IP_PROTO_UDP:
            udp_handle(payload, payload_len, ip->src_ip);
            break;

        case IP_PROTO_TCP:
            tcp_handle(payload, payload_len, ip->src_ip);
            break;
    }
}

// Handle ICMP packet
void icmp_handle(const uint8_t *packet, size_t length, uint32_t src_ip) {
    if (length < sizeof(icmp_header_t)) return;

    const icmp_header_t *icmp = (const icmp_header_t *)packet;

    if (icmp->type == ICMP_ECHO_REQUEST) {
        // Send echo reply
        size_t reply_len = length;
        uint8_t reply[reply_len];

        memcpy(reply, packet, length);
        icmp_header_t *reply_icmp = (icmp_header_t *)reply;

        reply_icmp->type = ICMP_ECHO_REPLY;
        reply_icmp->checksum = 0;
        reply_icmp->checksum = ip_checksum(reply, reply_len);

        ip_send(src_ip, IP_PROTO_ICMP, reply, reply_len);
    }
    else if (icmp->type == ICMP_ECHO_REPLY) {
        // Check if this is a reply to our ping
        if (ntohs(icmp->id) == icmp_last_id &&
            ntohs(icmp->sequence) == icmp_last_seq) {
            icmp_reply_received = 1;
        }
    }
}

// Send ICMP echo request
int icmp_send_echo_request(uint32_t dest_ip, uint16_t id, uint16_t seq) {
    uint8_t packet[sizeof(icmp_header_t) + 32];  // 32 bytes of data

    icmp_header_t *icmp = (icmp_header_t *)packet;
    icmp->type = ICMP_ECHO_REQUEST;
    icmp->code = 0;
    icmp->id = htons(id);
    icmp->sequence = htons(seq);
    icmp->checksum = 0;

    // Fill data
    for (int i = 0; i < 32; i++) {
        packet[sizeof(icmp_header_t) + i] = i;
    }

    icmp->checksum = ip_checksum(packet, sizeof(packet));

    icmp_last_id = id;
    icmp_last_seq = seq;
    icmp_reply_received = 0;

    return ip_send(dest_ip, IP_PROTO_ICMP, packet, sizeof(packet));
}

// Ping function
int ping(uint32_t dest_ip, int count) {
    int success = 0;

    // Check link status
    if (!e1000_link_up()) {
        print_str("Error: Network link is down\n");
        return 0;
    }

    uint8_t ip_bytes[4];
    uint32_to_ip(dest_ip, ip_bytes);

    for (int i = 0; i < count; i++) {
        icmp_reply_received = 0;

        int send_result = icmp_send_echo_request(dest_ip, 1, i + 1);
        if (send_result < 0) {
            print_str("Failed to send ICMP request (ARP failed?)\n");
            continue;
        }

        // Wait for reply - use much longer timeout
        // ~3 second timeout per ping (30000 iterations * 100000 delay)
        for (int j = 0; j < 30000 && !icmp_reply_received; j++) {
            net_process_packet();
            for (volatile int k = 0; k < 100000; k++);  // Longer delay
        }

        if (icmp_reply_received) {
            success++;
            print_str("Reply from ");
            print_int(ip_bytes[0]); print_str(".");
            print_int(ip_bytes[1]); print_str(".");
            print_int(ip_bytes[2]); print_str(".");
            print_int(ip_bytes[3]); print_str(": seq=");
            print_int(i + 1);
            print_str("\n");
        } else {
            print_str("Request timeout for seq ");
            print_int(i + 1);
            print_str("\n");
        }
    }

    return success;
}

// Process incoming packets
void net_process_packet(void) {
    int len = e1000_receive_packet(packet_buffer, sizeof(packet_buffer));
    if (len > 0) {
        net_handle_packet(packet_buffer, len);
    }
}

// Handle received packet
int net_handle_packet(uint8_t *buffer, size_t length) {
    if (length < sizeof(eth_header_t)) return -1;

    eth_header_t *eth = (eth_header_t *)buffer;
    uint16_t type = ntohs(eth->type);

    switch (type) {
        case ETH_TYPE_ARP:
            arp_handle(buffer, length);
            break;

        case ETH_TYPE_IPV4:
            ip_handle(buffer, length);
            break;
    }

    return 0;
}
