// help command - display available commands

#include <shell/commands.h>
#include <shell/print.h>

void cmd_help(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    const command_t *commands = commands_get_table();
    int count = commands_get_count();

    print_str("Available commands:\n");
    for (int i = 0; i < count; i++)
    {
        print_str("  ");
        print_str((char *)commands[i].name);
        print_str(" - ");
        print_str((char *)commands[i].description);
        print_str("\n");
    }
}
