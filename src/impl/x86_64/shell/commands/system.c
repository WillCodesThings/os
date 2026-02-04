// System commands: heap, pci

#include <shell/commands.h>
#include <shell/print.h>
#include <memory/heap.h>
#include <drivers/pci.h>

void cmd_heap(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    uint32_t total, used, free_mem;
    heap_stats(&total, &used, &free_mem);

    if (total == 0)
    {
        print_str("Heap not initialized\n");
        return;
    }

    print_str("Heap Statistics:\n");
    print_str("  Total: ");
    print_uint(total / 1024);
    print_str(" KB\n");

    print_str("  Used:  ");
    print_uint(used / 1024);
    print_str(" KB (");
    print_uint((used * 100) / total);
    print_str("%)\n");

    print_str("  Free:  ");
    print_uint(free_mem / 1024);
    print_str(" KB (");
    print_uint((free_mem * 100) / total);
    print_str("%)\n");
}

void cmd_pci(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    int count = pci_get_device_count();
    print_str("PCI Devices (");
    print_int(count);
    print_str(" found):\n");

    for (int i = 0; i < count; i++)
    {
        pci_device_t *dev = pci_get_device(i);
        if (!dev)
            continue;

        print_str("  ");
        print_int(dev->bus);
        print_str(":");
        print_int(dev->device);
        print_str(".");
        print_int(dev->function);
        print_str(" - Vendor: 0x");
        print_hex(dev->vendor_id);
        print_str(" Device: 0x");
        print_hex(dev->device_id);
        print_str(" Class: ");
        print_hex(dev->class_code);
        print_str(":");
        print_hex(dev->subclass);
        print_str("\n");
    }
}
