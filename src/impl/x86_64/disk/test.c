#include <stdint.h>
#include <shell/print.h>
#include <interrupts/io/ata.h>
#include <interrupts/idt.h>
#include <interrupts/port_io.h>

void ata_irq_test(void)
{
    serial_print("\n=== ATA IRQ Test ===\n");

    uint16_t buf[512] = {0};

    // Try polling first (bypass interrupts)
    serial_print("Polling sector 0 on primary master...\n");
    if (ata_read_sector(ATA_PRIMARY_MASTER, 0, buf) == 0)
    {
        serial_print("Polling read successful!\nFirst 16 bytes: ");
        for (int i = 0; i < 16; i++)
        {
            serial_print_hex(buf[i]);
            serial_print(" ");
        }
        serial_print("\n");
    }
    else
    {
        serial_print("Polling read failed!\n");
    }

    // Now test IRQ-based read
    serial_print("IRQ-based read test (wait for interrupt)...\n");

    // Reset irq flag before read
    extern ata_device_t ata_primary;
    ata_primary.irq_invoked = 0;

    if (ata_read_sector(ATA_PRIMARY_MASTER, 0, buf) == 0)
    {
        serial_print("IRQ read successful!\nFirst 16 bytes: ");
        for (int i = 0; i < 16; i++)
        {
            serial_print_hex(buf[i]);
            serial_print(" ");
        }
        serial_print("\n");
    }
    else
    {
        serial_print("IRQ read failed! Timeout likely.\n");
    }

    serial_print("=== ATA IRQ Test Complete ===\n");
}
