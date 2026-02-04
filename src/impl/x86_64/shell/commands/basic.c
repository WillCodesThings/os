// Basic commands: clear, info, reboot, echo

#include <shell/commands.h>
#include <shell/print.h>
#include <interrupts/port_io.h>

void cmd_clear(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    print_clear();
}

void cmd_info(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    print_str("Simple OS Shell\n");
    print_str("Version 0.2\n");
    print_str("System is running!\n");
}

void cmd_reboot(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    print_str("Rebooting system...\n");
    uint8_t good = 0x02;
    while (good & 0x02)
    {
        good = inb(0x64);
    }
    outb(0x64, 0xFE);
}

void cmd_echo(int argc, char **argv)
{
    for (int i = 1; i < argc; i++)
    {
        print_str(argv[i]);
        if (i < argc - 1)
        {
            print_str(" ");
        }
    }
    print_str("\n");
}
