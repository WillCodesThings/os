#pragma once

#include <stdint.h>

// Command function type
typedef void (*command_function)(int argc, char **argv);

// Command structure
typedef struct {
    const char *name;
    const char *description;
    command_function function;
} command_t;

// Maximum number of commands
#define MAX_COMMANDS 32

// Get the command table
const command_t *commands_get_table(void);

// Get number of registered commands
int commands_get_count(void);

// Shell utility functions (available to all commands)
int shell_get_stdin_len(void);
const char *shell_get_stdin(void);

// String conversion utility (defined in shell.c)
int atoi(const char *str);

// Command declarations - implemented in commands/*.c files
void cmd_help(int argc, char **argv);
void cmd_clear(int argc, char **argv);
void cmd_info(int argc, char **argv);
void cmd_reboot(int argc, char **argv);
void cmd_draw(int argc, char **argv);
void cmd_cls(int argc, char **argv);
void cmd_ls(int argc, char **argv);
void cmd_cat(int argc, char **argv);
void cmd_heap(int argc, char **argv);
void cmd_touch(int argc, char **argv);
void cmd_rm(int argc, char **argv);
void cmd_write(int argc, char **argv);
void cmd_echo(int argc, char **argv);
void cmd_ping(int argc, char **argv);
void cmd_ifconfig(int argc, char **argv);
void cmd_pci(int argc, char **argv);
void cmd_netstat(int argc, char **argv);
void cmd_wget(int argc, char **argv);
void cmd_view(int argc, char **argv);
void cmd_run(int argc, char **argv);
void cmd_gui(int argc, char **argv);
void cmd_edit(int argc, char **argv);
void cmd_grep(int argc, char **argv);
