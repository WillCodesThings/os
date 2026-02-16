#include <disk/test.h>
#include <stdint.h>
#include <utils/log.h>
#include <drivers/serial.h>
#include <interrupts/io/ata.h>
#include <interrupts/idt.h>
#include <interrupts/port_io.h>

void ata_irq_test(void)
{
    LOG_DEBUG("DISK", "=== ATA IRQ Test ===");

    uint8_t buf[512] = {0};

    // Try polling first (bypass interrupts)
    LOG_DEBUG("DISK", "Polling sector 0 on primary master...");
    if (ata_read_sector(ATA_PRIMARY_MASTER, 0, buf) == 0)
    {
        LOG_DEBUG("DISK", "Polling read successful! First 16 bytes:");
        for (int i = 0; i < 16; i++)
        {
            serial_print_hex(buf[i]);
            serial_print(" ");
        }
        serial_print("\n");
    }
    else
    {
        LOG_DEBUG("DISK", "Polling read failed!");
    }

    // Now test IRQ-based read
    LOG_DEBUG("DISK", "IRQ-based read test (wait for interrupt)...");

    // Reset irq flag before read
    extern ata_device_t ata_primary;
    ata_primary.irq_invoked = 0;

    if (ata_read_sector(ATA_PRIMARY_MASTER, 0, buf) == 0)
    {
        LOG_DEBUG("DISK", "IRQ read successful! First 16 bytes:");
        for (int i = 0; i < 16; i++)
        {
            serial_print_hex(buf[i]);
            serial_print(" ");
        }
        serial_print("\n");
    }
    else
    {
        LOG_DEBUG("DISK", "IRQ read failed! Timeout likely.");
    }

    LOG_DEBUG("DISK", "=== ATA IRQ Test Complete ===");
}
