#include "drivers/e1000.h"
#include "drivers/pci.h"
#include "interrupts/port_io.h"
#include "interrupts/idt.h"
#include "memory/heap.h"
#include "utils/memory.h"

// Global e1000 device
static e1000_device_t e1000_dev;

// Forward declaration
extern void e1000_interrupt_handler(void);

// Read from MMIO
static uint32_t e1000_read(uint32_t reg) {
    return *((volatile uint32_t *)(e1000_dev.mmio_base + reg));
}

// Write to MMIO
static void e1000_write(uint32_t reg, uint32_t value) {
    *((volatile uint32_t *)(e1000_dev.mmio_base + reg)) = value;
}

// Read MAC address from EEPROM
static int e1000_read_eeprom(uint8_t addr, uint16_t *data) {
    uint32_t temp;

    // Write read command
    e1000_write(E1000_EERD, (1) | ((uint32_t)addr << 8));

    // Wait for completion
    for (int i = 0; i < 1000; i++) {
        temp = e1000_read(E1000_EERD);
        if (temp & (1 << 4)) {  // Done bit
            *data = (uint16_t)((temp >> 16) & 0xFFFF);
            return 0;
        }
    }

    return -1;
}

// Read MAC address
static void e1000_read_mac_address(void) {
    uint16_t word;

    // Try reading from EEPROM first
    if (e1000_read_eeprom(0, &word) == 0) {
        e1000_dev.mac_addr[0] = word & 0xFF;
        e1000_dev.mac_addr[1] = (word >> 8) & 0xFF;

        e1000_read_eeprom(1, &word);
        e1000_dev.mac_addr[2] = word & 0xFF;
        e1000_dev.mac_addr[3] = (word >> 8) & 0xFF;

        e1000_read_eeprom(2, &word);
        e1000_dev.mac_addr[4] = word & 0xFF;
        e1000_dev.mac_addr[5] = (word >> 8) & 0xFF;
    } else {
        // Read from RAL/RAH registers
        uint32_t ral = e1000_read(E1000_RAL);
        uint32_t rah = e1000_read(E1000_RAH);

        e1000_dev.mac_addr[0] = ral & 0xFF;
        e1000_dev.mac_addr[1] = (ral >> 8) & 0xFF;
        e1000_dev.mac_addr[2] = (ral >> 16) & 0xFF;
        e1000_dev.mac_addr[3] = (ral >> 24) & 0xFF;
        e1000_dev.mac_addr[4] = rah & 0xFF;
        e1000_dev.mac_addr[5] = (rah >> 8) & 0xFF;
    }
}

// Initialize RX descriptors
static int e1000_init_rx(void) {
    // Allocate descriptor ring (128-byte aligned for best compatibility)
    e1000_dev.rx_descs = (e1000_rx_desc_t *)kmalloc_aligned(
        sizeof(e1000_rx_desc_t) * E1000_NUM_RX_DESC, 128);

    if (!e1000_dev.rx_descs) return -1;

    memset(e1000_dev.rx_descs, 0, sizeof(e1000_rx_desc_t) * E1000_NUM_RX_DESC);

    // Allocate buffers for each descriptor
    for (int i = 0; i < E1000_NUM_RX_DESC; i++) {
        e1000_dev.rx_buffers[i] = (uint8_t *)kmalloc_aligned(E1000_RX_BUFFER_SIZE, 16);
        if (!e1000_dev.rx_buffers[i]) return -1;

        e1000_dev.rx_descs[i].addr = (uint64_t)e1000_dev.rx_buffers[i];
        e1000_dev.rx_descs[i].status = 0;
    }

    // Set up descriptor ring
    e1000_write(E1000_RDBAL, (uint32_t)((uint64_t)e1000_dev.rx_descs & 0xFFFFFFFF));
    e1000_write(E1000_RDBAH, (uint32_t)((uint64_t)e1000_dev.rx_descs >> 32));
    e1000_write(E1000_RDLEN, E1000_NUM_RX_DESC * sizeof(e1000_rx_desc_t));
    e1000_write(E1000_RDH, 0);
    e1000_write(E1000_RDT, E1000_NUM_RX_DESC - 1);

    e1000_dev.rx_cur = 0;

    // Enable receiver
    uint32_t rctl = E1000_RCTL_EN |
                    E1000_RCTL_BAM |        // Accept broadcast
                    E1000_RCTL_BSIZE_2048 | // 2048 byte buffers
                    E1000_RCTL_SECRC;       // Strip CRC

    e1000_write(E1000_RCTL, rctl);

    return 0;
}

// Initialize TX descriptors
static int e1000_init_tx(void) {
    // Allocate descriptor ring (128-byte aligned for best compatibility)
    e1000_dev.tx_descs = (e1000_tx_desc_t *)kmalloc_aligned(
        sizeof(e1000_tx_desc_t) * E1000_NUM_TX_DESC, 128);

    if (!e1000_dev.tx_descs) return -1;

    memset(e1000_dev.tx_descs, 0, sizeof(e1000_tx_desc_t) * E1000_NUM_TX_DESC);

    // Allocate buffers for each descriptor
    for (int i = 0; i < E1000_NUM_TX_DESC; i++) {
        e1000_dev.tx_buffers[i] = (uint8_t *)kmalloc_aligned(E1000_TX_BUFFER_SIZE, 16);
        if (!e1000_dev.tx_buffers[i]) return -1;

        e1000_dev.tx_descs[i].addr = (uint64_t)e1000_dev.tx_buffers[i];
        e1000_dev.tx_descs[i].status = E1000_TXD_STAT_DD;  // Mark as done initially
        e1000_dev.tx_descs[i].cmd = 0;
    }

    // Set up descriptor ring
    e1000_write(E1000_TDBAL, (uint32_t)((uint64_t)e1000_dev.tx_descs & 0xFFFFFFFF));
    e1000_write(E1000_TDBAH, (uint32_t)((uint64_t)e1000_dev.tx_descs >> 32));
    e1000_write(E1000_TDLEN, E1000_NUM_TX_DESC * sizeof(e1000_tx_desc_t));
    e1000_write(E1000_TDH, 0);
    e1000_write(E1000_TDT, 0);

    e1000_dev.tx_cur = 0;

    // Set Transmit Inter-Packet Gap (required for TX to work)
    // IPGT=10, IPGR1=10, IPGR2=10 for IEEE 802.3 standard
    e1000_write(E1000_TIPG, (10 << 0) | (10 << 10) | (10 << 20));

    // Enable transmitter
    uint32_t tctl = E1000_TCTL_EN |
                    E1000_TCTL_PSP |           // Pad short packets
                    (15 << E1000_TCTL_CT_SHIFT) | // Collision threshold
                    (64 << E1000_TCTL_COLD_SHIFT); // Collision distance

    e1000_write(E1000_TCTL, tctl);

    return 0;
}

// Initialize e1000 device
int e1000_init(void) {
    memset(&e1000_dev, 0, sizeof(e1000_dev));

    // Find e1000 device
    pci_device_t *pci = pci_find_device(PCI_VENDOR_INTEL, PCI_DEVICE_E1000);
    if (!pci) {
        pci = pci_find_device(PCI_VENDOR_INTEL, PCI_DEVICE_E1000_2);
    }
    if (!pci) {
        pci = pci_find_device(PCI_VENDOR_INTEL, PCI_DEVICE_E1000_3);
    }
    if (!pci) {
        // Try finding by class
        pci = pci_find_class(PCI_CLASS_NETWORK, PCI_SUBCLASS_ETHERNET);
    }

    if (!pci) {
        return -1;  // No network card found
    }

    e1000_dev.pci_dev = pci;

    // Enable bus mastering and memory space
    pci_enable_bus_mastering(pci);
    pci_enable_memory_space(pci);

    // Get MMIO base address from BAR0 (may be 64-bit)
    e1000_dev.mmio_base = pci_get_bar_address64(pci, 0);
    if (!e1000_dev.mmio_base) {
        return -2;  // No MMIO base
    }

    // Reset the device
    e1000_write(E1000_CTRL, e1000_read(E1000_CTRL) | E1000_CTRL_RST);

    // Wait for reset to complete
    for (volatile int i = 0; i < 100000; i++);

    // Wait for reset bit to clear
    while (e1000_read(E1000_CTRL) & E1000_CTRL_RST) {
        for (volatile int i = 0; i < 1000; i++);
    }

    // Disable interrupts during setup
    e1000_write(E1000_IMC, 0xFFFFFFFF);

    // Clear pending interrupts
    e1000_read(E1000_ICR);

    // Read MAC address
    e1000_read_mac_address();

    // Set link up
    e1000_write(E1000_CTRL, e1000_read(E1000_CTRL) | E1000_CTRL_SLU);

    // Clear multicast table
    for (int i = 0; i < 128; i++) {
        e1000_write(E1000_MTA + i * 4, 0);
    }

    // Initialize RX
    if (e1000_init_rx() != 0) {
        return -3;
    }

    // Initialize TX
    if (e1000_init_tx() != 0) {
        return -4;
    }

    // Set up interrupt handler
    uint8_t irq = pci->interrupt_line;
    if (irq > 0 && irq < 16) {
        idt_set_gate(0x20 + irq, (uint64_t)e1000_interrupt_handler);
    }

    // Enable interrupts
    e1000_write(E1000_IMS, E1000_ICR_RXT0 | E1000_ICR_LSC);

    e1000_dev.initialized = 1;

    return 0;
}

// Send a packet
int e1000_send_packet(const void *data, size_t length) {
    if (!e1000_dev.initialized || !data || length == 0) {
        return -1;
    }

    if (length > E1000_TX_BUFFER_SIZE) {
        return -2;  // Packet too large
    }

    uint16_t cur = e1000_dev.tx_cur;
    e1000_tx_desc_t *desc = &e1000_dev.tx_descs[cur];

    // Wait for descriptor to be available (with timeout)
    int timeout = 100000;
    while (!(desc->status & E1000_TXD_STAT_DD) && timeout > 0) {
        timeout--;
    }
    if (timeout == 0) {
        return -3;  // Timeout waiting for descriptor
    }

    // Copy data to buffer
    memcpy(e1000_dev.tx_buffers[cur], data, length);

    // Set up descriptor - use the buffer's address
    desc->addr = (uint64_t)(uintptr_t)e1000_dev.tx_buffers[cur];
    desc->length = length;
    desc->cmd = E1000_TXD_CMD_EOP | E1000_TXD_CMD_IFCS | E1000_TXD_CMD_RS;
    desc->status = 0;
    desc->cso = 0;
    desc->css = 0;
    desc->special = 0;

    // Advance tail
    e1000_dev.tx_cur = (cur + 1) % E1000_NUM_TX_DESC;
    e1000_write(E1000_TDT, e1000_dev.tx_cur);

    // Wait for transmission to complete
    timeout = 100000;
    while (!(desc->status & E1000_TXD_STAT_DD) && timeout > 0) {
        timeout--;
    }

    if (!(desc->status & E1000_TXD_STAT_DD)) {
        return -4;  // TX didn't complete
    }

    return length;
}

// Receive a packet
int e1000_receive_packet(void *buffer, size_t max_length) {
    if (!e1000_dev.initialized || !buffer) {
        return -1;
    }

    uint16_t cur = e1000_dev.rx_cur;
    e1000_rx_desc_t *desc = &e1000_dev.rx_descs[cur];

    // Check if packet available
    if (!(desc->status & E1000_RXD_STAT_DD)) {
        return 0;  // No packet
    }

    // Get packet length
    uint16_t length = desc->length;
    if (length > max_length) {
        length = max_length;
    }

    // Copy data
    memcpy(buffer, e1000_dev.rx_buffers[cur], length);

    // Reset descriptor
    desc->status = 0;

    // Advance head
    uint16_t old_cur = cur;
    e1000_dev.rx_cur = (cur + 1) % E1000_NUM_RX_DESC;

    // Update tail
    e1000_write(E1000_RDT, old_cur);

    return length;
}

// Get MAC address
void e1000_get_mac_address(uint8_t *mac) {
    if (mac) {
        memcpy(mac, e1000_dev.mac_addr, 6);
    }
}

// Handle interrupt
void e1000_handle_interrupt(void) {
    uint32_t icr = e1000_read(E1000_ICR);

    if (icr & E1000_ICR_LSC) {
        // Link status change
        // Re-read link status if needed
    }

    if (icr & E1000_ICR_RXT0) {
        // Packet received - handled in polling for now
    }
}

// Check if link is up
int e1000_link_up(void) {
    if (!e1000_dev.initialized) return 0;
    return (e1000_read(E1000_STATUS) & 0x02) != 0;
}

// Get MMIO base address (for debugging)
uint64_t e1000_get_mmio_base(void) {
    return e1000_dev.mmio_base;
}

// Get status register (for debugging)
uint32_t e1000_get_status(void) {
    if (!e1000_dev.initialized) return 0;
    return e1000_read(E1000_STATUS);
}

// Debug: print TX state
void e1000_debug_tx(void) {
    if (!e1000_dev.initialized) return;

    extern void print_str(char *);
    extern void print_int(int);

    print_str("TX Debug:\n");
    print_str("  TCTL: ");
    print_int(e1000_read(E1000_TCTL));
    print_str("\n  TDH: ");
    print_int(e1000_read(E1000_TDH));
    print_str(" TDT: ");
    print_int(e1000_read(E1000_TDT));
    print_str("\n  TDBAL: ");
    print_int(e1000_read(E1000_TDBAL));
    print_str("\n  tx_descs addr: ");
    print_int((uint32_t)(uintptr_t)e1000_dev.tx_descs);
    print_str("\n  tx_buf[0] addr: ");
    print_int((uint32_t)(uintptr_t)e1000_dev.tx_buffers[0]);
    print_str("\n  desc[0].status: ");
    print_int(e1000_dev.tx_descs[0].status);
    print_str("\n");
}
