#pragma once
#include <stdint.h>
#include <stddef.h>
#include "drivers/pci.h"

// e1000 Register Offsets
#define E1000_CTRL      0x0000  // Device Control
#define E1000_STATUS    0x0008  // Device Status
#define E1000_EECD      0x0010  // EEPROM/Flash Control
#define E1000_EERD      0x0014  // EEPROM Read
#define E1000_CTRL_EXT  0x0018  // Extended Device Control
#define E1000_ICR       0x00C0  // Interrupt Cause Read
#define E1000_IMS       0x00D0  // Interrupt Mask Set
#define E1000_IMC       0x00D8  // Interrupt Mask Clear
#define E1000_RCTL      0x0100  // Receive Control
#define E1000_TCTL      0x0400  // Transmit Control
#define E1000_TIPG      0x0410  // Transmit Inter-Packet Gap
#define E1000_RDBAL     0x2800  // RX Descriptor Base Low
#define E1000_RDBAH     0x2804  // RX Descriptor Base High
#define E1000_RDLEN     0x2808  // RX Descriptor Length
#define E1000_RDH       0x2810  // RX Descriptor Head
#define E1000_RDT       0x2818  // RX Descriptor Tail
#define E1000_TDBAL     0x3800  // TX Descriptor Base Low
#define E1000_TDBAH     0x3804  // TX Descriptor Base High
#define E1000_TDLEN     0x3808  // TX Descriptor Length
#define E1000_TDH       0x3810  // TX Descriptor Head
#define E1000_TDT       0x3818  // TX Descriptor Tail
#define E1000_RAL       0x5400  // Receive Address Low
#define E1000_RAH       0x5404  // Receive Address High
#define E1000_MTA       0x5200  // Multicast Table Array

// CTRL Register bits
#define E1000_CTRL_SLU      (1 << 6)   // Set Link Up
#define E1000_CTRL_RST      (1 << 26)  // Device Reset

// RCTL Register bits
#define E1000_RCTL_EN       (1 << 1)   // Receiver Enable
#define E1000_RCTL_SBP      (1 << 2)   // Store Bad Packets
#define E1000_RCTL_UPE      (1 << 3)   // Unicast Promiscuous
#define E1000_RCTL_MPE      (1 << 4)   // Multicast Promiscuous
#define E1000_RCTL_LPE      (1 << 5)   // Long Packet Enable
#define E1000_RCTL_BAM      (1 << 15)  // Broadcast Accept Mode
#define E1000_RCTL_BSIZE_2048 (0 << 16) // Buffer size 2048
#define E1000_RCTL_SECRC    (1 << 26)  // Strip Ethernet CRC

// TCTL Register bits
#define E1000_TCTL_EN       (1 << 1)   // Transmitter Enable
#define E1000_TCTL_PSP      (1 << 3)   // Pad Short Packets
#define E1000_TCTL_CT_SHIFT 4          // Collision Threshold
#define E1000_TCTL_COLD_SHIFT 12       // Collision Distance

// Interrupt bits
#define E1000_ICR_TXDW      (1 << 0)   // TX Descriptor Written Back
#define E1000_ICR_TXQE      (1 << 1)   // TX Queue Empty
#define E1000_ICR_LSC       (1 << 2)   // Link Status Change
#define E1000_ICR_RXDMT0    (1 << 4)   // RX Descriptor Minimum Threshold
#define E1000_ICR_RXO       (1 << 6)   // RX Overrun
#define E1000_ICR_RXT0      (1 << 7)   // RX Timer Interrupt

// TX Descriptor Command bits
#define E1000_TXD_CMD_EOP   (1 << 0)   // End of Packet
#define E1000_TXD_CMD_IFCS  (1 << 1)   // Insert FCS
#define E1000_TXD_CMD_RS    (1 << 3)   // Report Status

// TX Descriptor Status bits
#define E1000_TXD_STAT_DD   (1 << 0)   // Descriptor Done

// RX Descriptor Status bits
#define E1000_RXD_STAT_DD   (1 << 0)   // Descriptor Done
#define E1000_RXD_STAT_EOP  (1 << 1)   // End of Packet

// Descriptor counts (must be multiple of 8)
#define E1000_NUM_RX_DESC 32
#define E1000_NUM_TX_DESC 32

// Buffer sizes
#define E1000_RX_BUFFER_SIZE 2048
#define E1000_TX_BUFFER_SIZE 2048

// RX Descriptor
typedef struct {
    uint64_t addr;      // Buffer address
    uint16_t length;    // Packet length
    uint16_t checksum;  // Packet checksum
    uint8_t status;     // Status
    uint8_t errors;     // Errors
    uint16_t special;   // Special
} __attribute__((packed)) e1000_rx_desc_t;

// TX Descriptor
typedef struct {
    uint64_t addr;      // Buffer address
    uint16_t length;    // Packet length
    uint8_t cso;        // Checksum offset
    uint8_t cmd;        // Command
    uint8_t status;     // Status
    uint8_t css;        // Checksum start
    uint16_t special;   // Special
} __attribute__((packed)) e1000_tx_desc_t;

// e1000 Device structure
typedef struct {
    pci_device_t *pci_dev;
    uint64_t mmio_base;         // Memory-mapped I/O base
    uint8_t mac_addr[6];        // MAC address

    // RX ring
    e1000_rx_desc_t *rx_descs;
    uint8_t *rx_buffers[E1000_NUM_RX_DESC];
    uint16_t rx_cur;

    // TX ring
    e1000_tx_desc_t *tx_descs;
    uint8_t *tx_buffers[E1000_NUM_TX_DESC];
    uint16_t tx_cur;

    int initialized;
} e1000_device_t;

// e1000 functions
int e1000_init(void);
int e1000_send_packet(const void *data, size_t length);
int e1000_receive_packet(void *buffer, size_t max_length);
void e1000_get_mac_address(uint8_t *mac);
void e1000_handle_interrupt(void);
int e1000_link_up(void);
uint64_t e1000_get_mmio_base(void);
uint32_t e1000_get_status(void);
void e1000_debug_tx(void);
