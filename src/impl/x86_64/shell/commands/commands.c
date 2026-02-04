// Central command registration file
// This file builds the command table from all individual command modules

#include <shell/commands.h>

// Command table - registers all commands
static const command_t command_table[] = {
    {"help", "Display available commands", cmd_help},
    {"clear", "Clear the screen", cmd_clear},
    {"info", "Show system information", cmd_info},
    {"reboot", "Reboot the system", cmd_reboot},
    {"draw", "Draw shapes (draw triangle|rect|line|pixel)", cmd_draw},
    {"cls", "Clear the graphics screen", cmd_cls},
    {"ls", "List directory contents", cmd_ls},
    {"cat", "Display file contents", cmd_cat},
    {"heap", "Show heap statistics", cmd_heap},
    {"touch", "Create a new file", cmd_touch},
    {"rm", "Remove a file", cmd_rm},
    {"write", "Write text to a file", cmd_write},
    {"echo", "Print text to the screen", cmd_echo},
    {"ping", "Ping an IP address (ping <ip>)", cmd_ping},
    {"ifconfig", "Show/set network config (ifconfig [ip] [gateway])", cmd_ifconfig},
    {"pci", "List PCI devices", cmd_pci},
    {"netstat", "Show network status", cmd_netstat},
    {"wget", "Fetch URL content (wget <ip> <port> <path>)", cmd_wget},
    {"view", "Open image viewer (view <file.bmp>)", cmd_view},
    {"run", "Run an ELF binary (run <file>)", cmd_run},
    {"gui", "Launch GUI desktop with apps", cmd_gui},
    {"edit", "Open text editor (edit [file])", cmd_edit},
    {"grep", "Filter lines matching pattern (grep <pattern>)", cmd_grep},
    {0, 0, 0} // Sentinel
};

// Get the command table
const command_t *commands_get_table(void)
{
    return command_table;
}

// Get number of registered commands
int commands_get_count(void)
{
    int count = 0;
    while (command_table[count].name != 0)
    {
        count++;
    }
    return count;
}
