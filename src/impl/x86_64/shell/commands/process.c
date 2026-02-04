// Process commands: run, gui, edit

#include <shell/commands.h>
#include <shell/print.h>
#include <exec/process.h>
#include <gui/desktop.h>

void cmd_run(int argc, char **argv)
{
    if (argc < 2)
    {
        print_str("Usage: run <program>\n");
        return;
    }

    print_str("Running: ");
    print_str(argv[1]);
    print_str("\n");

    int ret = process_exec_file(argv[1]);
    if (ret < 0)
    {
        print_str("Failed to execute program (error ");
        print_int(-ret);
        print_str(")\n");
    }
    else
    {
        print_str("Program exited with code ");
        print_int(ret);
        print_str("\n");
    }
}

void cmd_gui(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    print_str("Launching Desktop Environment...\n");
    desktop_init();
    print_str("Desktop ready!\n");
    print_str("Click taskbar buttons to open apps.\n");
    print_str("Drag title bars to move windows.\n");
    print_str("Click X to close windows.\n");
}

void cmd_edit(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (!desktop_is_active())
    {
        desktop_init();
    }
    desktop_open_app(APP_EDITOR);
    print_str("Text editor opened!\n");
}
