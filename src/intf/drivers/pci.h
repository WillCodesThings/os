#pragma once
#include <stdint.h>
#include <stddef.h>

// PCI Configuration Space Ports
#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

// PCI Configuration Space Registers
#define PCI_VENDOR_ID       0x00
#define PCI_DEVICE_ID       0x02
#define PCI_COMMAND         0x04
#define PCI_STATUS          0x06
#define PCI_REVISION_ID     0x08
#define PCI_PROG_IF         0x09
#define PCI_SUBCLASS        0x0A
#define PCI_CLASS           0x0B
#define PCI_CACHE_LINE_SIZE 0x0C
#define PCI_LATENCY_TIMER   0x0D
#define PCI_HEADER_TYPE     0x0E
#define PCI_BIST            0x0F
#define PCI_BAR0            0x10
#define PCI_BAR1            0x14
#define PCI_BAR2            0x18
#define PCI_BAR3            0x1C
#define PCI_BAR4            0x20
#define PCI_BAR5            0x24
#define PCI_INTERRUPT_LINE  0x3C
#define PCI_INTERRUPT_PIN   0x3D

// PCI Command Register bits
#define PCI_COMMAND_IO          0x0001
#define PCI_COMMAND_MEMORY      0x0002
#define PCI_COMMAND_MASTER      0x0004
#define PCI_COMMAND_SPECIAL     0x0008
#define PCI_COMMAND_INVALIDATE  0x0010
#define PCI_COMMAND_VGA_PALETTE 0x0020
#define PCI_COMMAND_PARITY      0x0040
#define PCI_COMMAND_WAIT        0x0080
#define PCI_COMMAND_SERR        0x0100
#define PCI_COMMAND_FAST_BACK   0x0200
#define PCI_COMMAND_INTX_DISABLE 0x0400

// PCI Class Codes
#define PCI_CLASS_NETWORK 0x02

// PCI Subclass for Network
#define PCI_SUBCLASS_ETHERNET 0x00

// Known device IDs
#define PCI_VENDOR_INTEL    0x8086
#define PCI_DEVICE_E1000    0x100E  // Intel 82540EM (QEMU default)
#define PCI_DEVICE_E1000_2  0x100F  // Intel 82545EM
#define PCI_DEVICE_E1000_3  0x10D3  // Intel 82574L

// PCI Device structure
typedef struct {
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t class_code;
    uint8_t subclass;
    uint8_t prog_if;
    uint8_t revision;
    uint8_t header_type;
    uint8_t interrupt_line;
    uint8_t interrupt_pin;
    uint32_t bar[6];
} pci_device_t;

// Maximum PCI devices to track
#define MAX_PCI_DEVICES 64

// PCI functions
void pci_init(void);
uint32_t pci_read(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
void pci_write(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value);

uint16_t pci_read16(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
uint8_t pci_read8(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
void pci_write16(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint16_t value);

// Device discovery
int pci_scan(void);
pci_device_t *pci_find_device(uint16_t vendor_id, uint16_t device_id);
pci_device_t *pci_find_class(uint8_t class_code, uint8_t subclass);
pci_device_t *pci_get_device(int index);
int pci_get_device_count(void);

// Device configuration
void pci_enable_bus_mastering(pci_device_t *dev);
void pci_enable_memory_space(pci_device_t *dev);
void pci_enable_io_space(pci_device_t *dev);
uint32_t pci_get_bar_address(pci_device_t *dev, int bar);
uint64_t pci_get_bar_address64(pci_device_t *dev, int bar);
uint32_t pci_get_bar_size(pci_device_t *dev, int bar);
