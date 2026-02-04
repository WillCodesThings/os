// File commands: ls, cat, touch, rm, write

#include <shell/commands.h>
#include <shell/print.h>
#include <fs/vfs.h>
#include <memory/heap.h>

void cmd_ls(int argc, char **argv)
{
    const char *path = (argc > 1) ? argv[1] : "/";

    vfs_node_t *dir = vfs_resolve_path(path);
    if (!dir)
    {
        print_str("Directory not found\n");
        return;
    }

    if (!(dir->flags & VFS_DIRECTORY))
    {
        print_str("Not a directory\n");
        return;
    }

    print_str("Contents of ");
    print_str((char *)path);
    print_str(":\n");

    if (!dir->readdir)
    {
        print_str("Error: Directory listing not supported\n");
        return;
    }

    uint32_t i = 0;
    vfs_node_t *child;
    int file_count = 0;

    while (i < 64 && (child = vfs_readdir(dir, i)) != 0)
    {
        print_str("  ");
        if (child->flags & VFS_DIRECTORY)
            print_str("[DIR]  ");
        else
            print_str("[FILE] ");
        print_str(child->name);
        print_str("\n");

        kfree(child);
        file_count++;
        i++;
    }

    if (file_count == 0)
    {
        print_str("  (empty)\n");
    }
    else
    {
        print_str("\nTotal: ");
        print_int(file_count);
        print_str(" item(s)\n");
    }
}

void cmd_cat(int argc, char **argv)
{
    if (argc < 2)
    {
        print_str("Usage: cat <file>\n");
        return;
    }

    vfs_node_t *file = vfs_open(argv[1], VFS_READ);
    if (!file)
    {
        print_str("File not found\n");
        return;
    }

    uint8_t buffer[256];
    int bytes = vfs_read(file, 0, 256, buffer);

    if (bytes > 0)
    {
        for (int i = 0; i < bytes; i++)
        {
            print_char(buffer[i]);
        }
    }

    vfs_close(file);
}

void cmd_touch(int argc, char **argv)
{
    if (argc < 2)
    {
        print_str("Usage: touch <filename>\n");
        return;
    }

    vfs_node_t *root = vfs_get_root();
    if (!root)
    {
        print_str("Error: No filesystem mounted\n");
        return;
    }

    vfs_node_t *existing = vfs_finddir(root, argv[1]);
    if (existing)
    {
        print_str("File already exists: ");
        print_str(argv[1]);
        print_str("\n");
        kfree(existing);
        return;
    }

    if (root->create)
    {
        int result = root->create(root, argv[1], VFS_FILE);
        if (result == 0)
        {
            print_str("Created: ");
            print_str(argv[1]);
            print_str("\n");
        }
        else
        {
            print_str("Error creating file\n");
        }
    }
    else
    {
        print_str("Error: Filesystem does not support file creation\n");
    }
}

void cmd_rm(int argc, char **argv)
{
    if (argc < 2)
    {
        print_str("Usage: rm <filename>\n");
        return;
    }

    vfs_node_t *root = vfs_get_root();
    if (!root)
    {
        print_str("Error: No filesystem mounted\n");
        return;
    }

    vfs_node_t *existing = vfs_finddir(root, argv[1]);
    if (!existing)
    {
        print_str("File not found: ");
        print_str(argv[1]);
        print_str("\n");
        return;
    }
    kfree(existing);

    if (root->delete)
    {
        int result = root->delete(root, argv[1]);
        if (result == 0)
        {
            print_str("Removed: ");
            print_str(argv[1]);
            print_str("\n");
        }
        else
        {
            print_str("Error removing file\n");
        }
    }
    else
    {
        print_str("Error: Filesystem does not support file deletion\n");
    }
}

void cmd_write(int argc, char **argv)
{
    if (argc < 3)
    {
        print_str("Usage: write <filename> <text...>\n");
        return;
    }

    vfs_node_t *file = vfs_open(argv[1], VFS_WRITE);
    if (!file)
    {
        print_str("File not found: ");
        print_str(argv[1]);
        print_str("\nTip: Use 'touch' to create a file first\n");
        return;
    }

    char content[512];
    int pos = 0;
    for (int i = 2; i < argc && pos < 500; i++)
    {
        char *arg = argv[i];
        while (*arg && pos < 500)
        {
            content[pos++] = *arg++;
        }
        if (i < argc - 1 && pos < 500)
        {
            content[pos++] = ' ';
        }
    }
    content[pos] = '\0';

    int bytes = vfs_write(file, 0, pos, (uint8_t *)content);
    vfs_close(file);

    if (bytes > 0)
    {
        print_str("Wrote ");
        print_int(bytes);
        print_str(" bytes to ");
        print_str(argv[1]);
        print_str("\n");
    }
    else
    {
        print_str("Error writing to file\n");
    }
}
