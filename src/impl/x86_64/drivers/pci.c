#include "drivers/pci.h"
#include "interrupts/port_io.h"
#include "utils/memory.h"

// PCI device list
static pci_device_t pci_devices[MAX_PCI_DEVICES];
static int pci_device_count = 0;

// Build PCI address for configuration access
static uint32_t pci_address(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    return (1 << 31)                    // Enable bit
         | ((uint32_t)bus << 16)
         | ((uint32_t)device << 11)
         | ((uint32_t)function << 8)
         | (offset & 0xFC);             // Align to 4-byte boundary
}

uint32_t pci_read(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    outl(PCI_CONFIG_ADDRESS, pci_address(bus, device, function, offset));
    return inl(PCI_CONFIG_DATA);
}

void pci_write(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value) {
    outl(PCI_CONFIG_ADDRESS, pci_address(bus, device, function, offset));
    outl(PCI_CONFIG_DATA, value);
}

uint16_t pci_read16(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    uint32_t value = pci_read(bus, device, function, offset);
    return (value >> ((offset & 2) * 8)) & 0xFFFF;
}

uint8_t pci_read8(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    uint32_t value = pci_read(bus, device, function, offset);
    return (value >> ((offset & 3) * 8)) & 0xFF;
}

void pci_write16(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint16_t value) {
    uint32_t old = pci_read(bus, device, function, offset);
    int shift = (offset & 2) * 8;
    old &= ~(0xFFFF << shift);
    old |= (value << shift);
    pci_write(bus, device, function, offset, old);
}

// Check if device exists
static int pci_device_exists(uint8_t bus, uint8_t device, uint8_t function) {
    uint16_t vendor = pci_read16(bus, device, function, PCI_VENDOR_ID);
    return vendor != 0xFFFF;
}

// Add device to list
static void pci_add_device(uint8_t bus, uint8_t device, uint8_t function) {
    if (pci_device_count >= MAX_PCI_DEVICES) return;

    pci_device_t *dev = &pci_devices[pci_device_count];

    dev->bus = bus;
    dev->device = device;
    dev->function = function;

    dev->vendor_id = pci_read16(bus, device, function, PCI_VENDOR_ID);
    dev->device_id = pci_read16(bus, device, function, PCI_DEVICE_ID);
    dev->class_code = pci_read8(bus, device, function, PCI_CLASS);
    dev->subclass = pci_read8(bus, device, function, PCI_SUBCLASS);
    dev->prog_if = pci_read8(bus, device, function, PCI_PROG_IF);
    dev->revision = pci_read8(bus, device, function, PCI_REVISION_ID);
    dev->header_type = pci_read8(bus, device, function, PCI_HEADER_TYPE);
    dev->interrupt_line = pci_read8(bus, device, function, PCI_INTERRUPT_LINE);
    dev->interrupt_pin = pci_read8(bus, device, function, PCI_INTERRUPT_PIN);

    // Read BARs
    for (int i = 0; i < 6; i++) {
        dev->bar[i] = pci_read(bus, device, function, PCI_BAR0 + i * 4);
    }

    pci_device_count++;
}

// Scan a single function
static void pci_scan_function(uint8_t bus, uint8_t device, uint8_t function) {
    if (!pci_device_exists(bus, device, function)) return;

    pci_add_device(bus, device, function);

    // Check if this is a PCI-to-PCI bridge
    uint8_t class_code = pci_read8(bus, device, function, PCI_CLASS);
    uint8_t subclass = pci_read8(bus, device, function, PCI_SUBCLASS);

    if (class_code == 0x06 && subclass == 0x04) {
        // PCI-to-PCI bridge - scan secondary bus
        uint8_t secondary_bus = pci_read8(bus, device, function, 0x19);
        for (int dev = 0; dev < 32; dev++) {
            pci_scan_function(secondary_bus, dev, 0);
        }
    }
}

// Scan a single device (all functions)
static void pci_scan_device(uint8_t bus, uint8_t device) {
    if (!pci_device_exists(bus, device, 0)) return;

    pci_scan_function(bus, device, 0);

    // Check for multi-function device
    uint8_t header_type = pci_read8(bus, device, 0, PCI_HEADER_TYPE);
    if (header_type & 0x80) {
        for (int function = 1; function < 8; function++) {
            pci_scan_function(bus, device, function);
        }
    }
}

// Scan all PCI buses
int pci_scan(void) {
    pci_device_count = 0;
    memset(pci_devices, 0, sizeof(pci_devices));

    // Scan all buses
    for (int bus = 0; bus < 256; bus++) {
        for (int device = 0; device < 32; device++) {
            pci_scan_device(bus, device);
        }
    }

    return pci_device_count;
}

void pci_init(void) {
    pci_scan();
}

pci_device_t *pci_find_device(uint16_t vendor_id, uint16_t device_id) {
    for (int i = 0; i < pci_device_count; i++) {
        if (pci_devices[i].vendor_id == vendor_id &&
            pci_devices[i].device_id == device_id) {
            return &pci_devices[i];
        }
    }
    return 0;
}

pci_device_t *pci_find_class(uint8_t class_code, uint8_t subclass) {
    for (int i = 0; i < pci_device_count; i++) {
        if (pci_devices[i].class_code == class_code &&
            pci_devices[i].subclass == subclass) {
            return &pci_devices[i];
        }
    }
    return 0;
}

pci_device_t *pci_get_device(int index) {
    if (index >= 0 && index < pci_device_count) {
        return &pci_devices[index];
    }
    return 0;
}

int pci_get_device_count(void) {
    return pci_device_count;
}

void pci_enable_bus_mastering(pci_device_t *dev) {
    uint16_t command = pci_read16(dev->bus, dev->device, dev->function, PCI_COMMAND);
    command |= PCI_COMMAND_MASTER;
    pci_write16(dev->bus, dev->device, dev->function, PCI_COMMAND, command);
}

void pci_enable_memory_space(pci_device_t *dev) {
    uint16_t command = pci_read16(dev->bus, dev->device, dev->function, PCI_COMMAND);
    command |= PCI_COMMAND_MEMORY;
    pci_write16(dev->bus, dev->device, dev->function, PCI_COMMAND, command);
}

void pci_enable_io_space(pci_device_t *dev) {
    uint16_t command = pci_read16(dev->bus, dev->device, dev->function, PCI_COMMAND);
    command |= PCI_COMMAND_IO;
    pci_write16(dev->bus, dev->device, dev->function, PCI_COMMAND, command);
}

uint64_t pci_get_bar_address64(pci_device_t *dev, int bar) {
    if (bar < 0 || bar > 5) return 0;

    uint32_t bar_value = dev->bar[bar];

    if (bar_value & 1) {
        // I/O space
        return bar_value & 0xFFFFFFFC;
    } else {
        // Memory space - check if 64-bit BAR
        uint8_t type = (bar_value >> 1) & 0x3;
        uint64_t addr = bar_value & 0xFFFFFFF0;

        if (type == 2 && bar < 5) {
            // 64-bit BAR - combine with next BAR
            addr |= ((uint64_t)dev->bar[bar + 1]) << 32;
        }

        return addr;
    }
}

uint32_t pci_get_bar_address(pci_device_t *dev, int bar) {
    return (uint32_t)pci_get_bar_address64(dev, bar);
}

uint32_t pci_get_bar_size(pci_device_t *dev, int bar) {
    if (bar < 0 || bar > 5) return 0;

    uint8_t offset = PCI_BAR0 + bar * 4;

    // Save original value
    uint32_t original = pci_read(dev->bus, dev->device, dev->function, offset);

    // Write all 1s
    pci_write(dev->bus, dev->device, dev->function, offset, 0xFFFFFFFF);

    // Read back
    uint32_t size = pci_read(dev->bus, dev->device, dev->function, offset);

    // Restore original
    pci_write(dev->bus, dev->device, dev->function, offset, original);

    if (original & 1) {
        // I/O space
        size &= 0xFFFFFFFC;
    } else {
        // Memory space
        size &= 0xFFFFFFF0;
    }

    return ~size + 1;  // Invert and add 1 to get size
}
