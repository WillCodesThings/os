#include <fs/simplefs.h>
#include <disk/block_device.h>
#include <fs/vfs.h>
#include <memory/heap.h>
#include <shell/shell.h>
#include <utils/string.h>

simplefs_superblock_t simplefs_superblock;
vfs_node_t *simplefs_root = NULL;
simplefs_filesystem_t *simplefs_fs = NULL;

static block_device_t *simplefs_block_device = NULL;

// Forward declarations
static int simplefs_vfs_read(struct vfs_node *node, uint32_t offset, uint32_t size, uint8_t *buffer);
static int simplefs_vfs_write(struct vfs_node *node, uint32_t offset, uint32_t size, uint8_t *buffer);
static void simplefs_vfs_open(struct vfs_node *node, uint32_t flags);
static void simplefs_vfs_close(struct vfs_node *node);
static int simplefs_vfs_create(struct vfs_node *node, const char *name, uint32_t flags);
static int simplefs_vfs_delete(struct vfs_node *node, const char *name);
static struct vfs_node *simplefs_vfs_readdir(struct vfs_node *node, uint32_t index);
static struct vfs_node *simplefs_vfs_finddir(struct vfs_node *node, const char *name);

void simplefs_init(block_device_t *block_device)
{
    serial_print("Initializing SimpleFS.\n");

    // Allocate root VFS node
    simplefs_root = (vfs_node_t *)kmalloc(sizeof(vfs_node_t));
    simplefs_root->name[0] = '/';
    simplefs_root->name[1] = '\0';
    simplefs_root->flags = VFS_DIRECTORY;
    simplefs_root->inode = 0;
    simplefs_root->length = 0;
    simplefs_root->impl = 0;
    simplefs_root->parent = NULL;
    simplefs_root->mountpoint = NULL;

    // Set function pointers
    simplefs_root->read = simplefs_vfs_read;
    simplefs_root->write = simplefs_vfs_write;
    simplefs_root->open = simplefs_vfs_open;
    simplefs_root->close = simplefs_vfs_close;
    simplefs_root->readdir = simplefs_vfs_readdir;
    simplefs_root->finddir = simplefs_vfs_finddir;
    simplefs_root->create = simplefs_vfs_create;
    simplefs_root->delete = simplefs_vfs_delete;

    // Allocate and configure filesystem object
    simplefs_fs = (simplefs_filesystem_t *)kmalloc(sizeof(simplefs_filesystem_t));
    simplefs_fs->name = "SimpleFS";
    simplefs_fs->mount = simplefs_mount;
    simplefs_fs->read_file = simplefs_read_file;
    simplefs_fs->write_file = simplefs_write_file;
    simplefs_fs->list_dir = simplefs_list_dir;
    simplefs_fs->find_file = simplefs_find_file;
    simplefs_fs->create_file = simplefs_create_file;
    simplefs_fs->delete_file = simplefs_delete_file;
    simplefs_fs->device = block_device;
    simplefs_fs->root = simplefs_root;

    // Attach filesystem to root VFS node
    simplefs_root->filesystem = simplefs_fs;

    // Mount the filesystem
    vfs_set_root(simplefs_root);
    simplefs_block_device = block_device;

    serial_print("SimpleFS initialized.\n");
}

void simplefs_format(block_device_t *block_device, uint32_t total_blocks, int reserved_blocks)
{
    serial_print("Formatting SimpleFS...\n");

    serial_print("DEBUG: Format entry - block_device = ");
    serial_print_hex((uint64_t)block_device);
    serial_print(", total_blocks = ");
    serial_print_hex(total_blocks);
    serial_print("\n");

    // --- Setup superblock ---
    simplefs_superblock_t sb = {0};
    sb.header.magic = 0x53465321;
    sb.header.version = 1;
    sb.block_size = block_device->block_size;
    sb.total_blocks = total_blocks;
    sb.free_block_count = sb.total_blocks - reserved_blocks;
    sb.first_data_block = reserved_blocks;
    sb.inode_count = 256;
    sb.free_inode_count = 256;
    sb.inodetable_start = 1;
    sb.inodetable_blocks = 10;
    sb.mount_count = 0;
    sb.last_mount_time = 0;
    sb.last_write_time = 0;

    serial_print("DEBUG: block_device->block_size = ");
    serial_print_hex(block_device->block_size);
    serial_print("\n");

    // Write superblock to block 0
    serial_print("DEBUG: About to write superblock\n");
    int result = block_device->write_block(block_device, 0, (uint8_t *)&sb);
    serial_print("DEBUG: Superblock write result = ");
    serial_print_hex(result);
    serial_print("\n");

    uint32_t dir_size = sizeof(simplefs_dir_entry_t) * ROOT_DIR_ENTRIES;

    serial_print("DEBUG: ROOT_DIR_ENTRIES = ");
    serial_print_hex(ROOT_DIR_ENTRIES);
    serial_print("\n");

    serial_print("DEBUG: sizeof(simplefs_dir_entry_t) = ");
    serial_print_hex(sizeof(simplefs_dir_entry_t));
    serial_print("\n");

    serial_print("DEBUG: Allocating ");
    serial_print_hex(dir_size);
    serial_print(" bytes for directory\n");

    // --- Allocate root directory ---
    simplefs_dir_entry_t *empty_entries = kmalloc(dir_size);
    if (!empty_entries)
    {
        serial_print("Failed to allocate directory buffer\n");
        return;
    }

    serial_print("DEBUG: Allocated at ");
    serial_print_hex((uint64_t)empty_entries);
    serial_print("\n");

    // Zero and mark entries as empty
    for (int i = 0; i < ROOT_DIR_ENTRIES; i++)
    {
        if (i % 8 == 0)
        {
            serial_print("Initializing entry ");
            serial_print_hex(i);
            serial_print(" at address ");
            serial_print_hex((uint64_t)&empty_entries[i]);
            serial_print("\n");
        }
        empty_entries[i].inode_number = 0xFFFFFFFF;
        empty_entries[i].name[0] = '\0';
        empty_entries[i].name_length = 0;
        empty_entries[i].file_type = 0;
        empty_entries[i].record_length = sizeof(simplefs_dir_entry_t);
    }

    serial_print("DEBUG: Finished initializing entries\n");

    // --- Write root directory to disk, block by block ---
    uint32_t blocks_needed = (dir_size + block_device->block_size - 1) / block_device->block_size;

    serial_print("DEBUG: Writing ");
    serial_print_hex(blocks_needed);
    serial_print(" directory blocks starting at block ");
    serial_print_hex(reserved_blocks);
    serial_print("\n");

    for (uint32_t i = 0; i < blocks_needed; i++)
    {
        serial_print("DEBUG: Writing directory block ");
        serial_print_hex(i);
        serial_print(" to disk block ");
        serial_print_hex(reserved_blocks + i);
        serial_print("\n");

        uint8_t *block_ptr = ((uint8_t *)empty_entries) + i * block_device->block_size;
        int result = block_device->write_block(block_device, reserved_blocks + i, block_ptr);

        serial_print("DEBUG: Write result = ");
        serial_print_hex(result);
        serial_print("\n");
    }

    serial_print("DEBUG: All directory blocks written\n");
    kfree(empty_entries);
    serial_print("SimpleFS formatted.\n");
}

int simplefs_mount(block_device_t *block_device)
{
    serial_print("Mounting SimpleFS...\n");

    if (!simplefs_fs)
    {
        serial_print("ERROR: simplefs_fs not initialized!\n");
        return -1;
    }

    simplefs_block_device = block_device;

    // Read superblock from disk
    simplefs_superblock_t sb;
    int result = block_device->read_block(block_device, 0, (uint8_t *)&sb);
    if (result != 0)
    {
        serial_print("Failed to read superblock\n");
        return -1;
    }

    serial_print("Magic: ");
    serial_print_hex(sb.header.magic);
    serial_print("\n");

    if (sb.header.magic != 0x53465321)
    {
        serial_print("Invalid filesystem magic!\n");
        return -1;
    }

    if (sb.header.version > 1)
    {
        serial_print("Unsupported filesystem version\n");
        return -1;
    }

    sb.max_inode_count = sb.inodetable_blocks * (sb.block_size / sizeof(simplefs_inode_t));

    // Cache superblock globally
    simplefs_superblock = sb;
    simplefs_fs->superblock = sb;
    simplefs_fs->device = block_device;

    serial_print("Mounted SimpleFS v");
    serial_print_hex(sb.header.version);
    serial_print("\n");

    return 0;
}

int simplefs_read_block(uint32_t block_number, void *buffer)
{
    if (!simplefs_block_device || !buffer)
        return -1;

    return simplefs_fs->device->read_block(simplefs_block_device, block_number, (uint8_t *)buffer);
}

int simplefs_read_file(uint32_t inode_number, uint8_t *buffer, uint32_t size, uint32_t offset)
{
    if (!simplefs_fs || !simplefs_fs->device || !buffer)
        return -1;

    simplefs_inode_t inode;
    simplefs_fs->device->read_block(simplefs_fs->device, simplefs_fs->superblock.inodetable_start + inode_number, (uint8_t *)&inode);

    if (inode.file_size == 0 || size == 0)
        return 0;

    uint32_t block_num = inode.direct_blocks[0];
    simplefs_fs->device->read_block(simplefs_fs->device, block_num, buffer);

    return inode.file_size;
}

int simplefs_write_file(uint32_t inode_number, const uint8_t *buffer, uint32_t size, uint32_t offset)
{
    if (!simplefs_fs || !simplefs_fs->device || !buffer)
        return -1;

    simplefs_inode_t inode;
    simplefs_fs->device->read_block(simplefs_fs->device, simplefs_fs->superblock.inodetable_start + inode_number, (uint8_t *)&inode);

    if (inode.direct_blocks[0] == 0)
    {
        // Allocate first block
        inode.direct_blocks[0] = simplefs_fs->superblock.first_data_block + inode_number;
    }

    simplefs_fs->device->write_block(simplefs_fs->device, inode.direct_blocks[0], (uint8_t *)buffer);

    inode.file_size = size;
    simplefs_fs->device->write_block(simplefs_fs->device, simplefs_fs->superblock.inodetable_start + inode_number, (uint8_t *)&inode);

    return size;
}

int simplefs_list_dir(uint32_t dir_inode_number)
{
    if (!simplefs_fs || !simplefs_fs->device)
        return -1;

    // Allocate correct size for all directory entries
    uint32_t dir_size = sizeof(simplefs_dir_entry_t) * ROOT_DIR_ENTRIES;
    simplefs_dir_entry_t *entries = (simplefs_dir_entry_t *)kmalloc(dir_size);
    if (!entries)
        return -1;

    simplefs_fs->device->read_block(simplefs_fs->device, simplefs_fs->superblock.first_data_block, (uint8_t *)entries);

    serial_print("Directory listing:\n");
    int count = 0;
    for (int i = 0; i < ROOT_DIR_ENTRIES; i++)
    {
        if (entries[i].inode_number != 0xFFFFFFFF)
        {
            serial_print(" - ");
            serial_print(entries[i].name);
            serial_print("\n");
            count++;
        }
    }

    if (count == 0)
    {
        serial_print(" (empty)\n");
    }

    kfree(entries);
    return 0;
}

int simplefs_create_file(uint32_t dir_inode_number, const char *filename, uint32_t *out_inode)
{
    if (!simplefs_fs || !filename || !out_inode)
        return -1;

    if (simplefs_fs->superblock.free_inode_count == 0)
        return -1;

    uint32_t inode_number = simplefs_fs->superblock.max_inode_count - simplefs_fs->superblock.free_inode_count;
    simplefs_fs->superblock.free_inode_count--;

    simplefs_inode_t new_inode = {0};
    simplefs_fs->device->write_block(simplefs_fs->device, simplefs_fs->superblock.inodetable_start + inode_number, (uint8_t *)&new_inode);

    // Add to root directory block
    simplefs_dir_entry_t *entries = (simplefs_dir_entry_t *)kmalloc(512);
    if (!entries)
        return -1;

    simplefs_fs->device->read_block(simplefs_fs->device, simplefs_fs->superblock.first_data_block, (uint8_t *)entries);

    // Find first empty slot
    for (int i = 0; i < 64; i++)
    {
        if (entries[i].inode_number == 0xFFFFFFFF)
        {
            entries[i].inode_number = inode_number;
            entries[i].name_length = strlen(filename);
            entries[i].file_type = 1;
            entries[i].record_length = sizeof(simplefs_dir_entry_t);
            strcpy(entries[i].name, filename);
            break;
        }
    }

    simplefs_fs->device->write_block(simplefs_fs->device, simplefs_fs->superblock.first_data_block, (uint8_t *)entries);
    kfree(entries);

    *out_inode = inode_number;
    return 0;
}

int simplefs_delete_file(uint32_t dir_inode_number, const char *filename)
{
    if (!simplefs_fs || !simplefs_fs->device || !filename)
        return -1;

    uint32_t inode_number;
    if (simplefs_find_file(dir_inode_number, filename, &inode_number) != 0)
        return -1;

    // Clear inode
    simplefs_inode_t empty_inode = {0};
    simplefs_fs->device->write_block(simplefs_fs->device, simplefs_fs->superblock.inodetable_start + inode_number, (uint8_t *)&empty_inode);

    // Clear directory entry
    simplefs_dir_entry_t *entries = (simplefs_dir_entry_t *)kmalloc(512);
    if (!entries)
        return -1;

    simplefs_fs->device->read_block(simplefs_fs->device, simplefs_fs->superblock.first_data_block, (uint8_t *)entries);

    for (int i = 0; i < 64; i++)
    {
        if (entries[i].inode_number == inode_number)
        {
            entries[i].inode_number = 0xFFFFFFFF;
            entries[i].name[0] = '\0';
            break;
        }
    }

    simplefs_fs->device->write_block(simplefs_fs->device, simplefs_fs->superblock.first_data_block, (uint8_t *)entries);
    kfree(entries);

    simplefs_fs->superblock.free_inode_count++;
    return 0;
}

void kernel_test_filesystem(void)
{
    serial_print("\n=== Filesystem Test Suite ===\n\n");

    // Test 1: List empty directory
    serial_print("[Test 1] Listing empty directory:\n");
    simplefs_list_dir(0);
    serial_print("\n");

    // Test 2: Create some test files
    serial_print("[Test 2] Creating test files...\n");

    const char *test_files[] = {
        "hello.txt",
        "readme.md",
        "config.sys",
        "boot.ini",
        "kernel.bin",
        "test.log",
        "data.dat",
        "notes.txt",
        "system.cfg",
        "temp.tmp"};

    uint32_t inode;
    for (int i = 0; i < 10; i++)
    {
        serial_print("  Creating: ");
        serial_print(test_files[i]);

        int result = simplefs_create_file(0, test_files[i], &inode);
        if (result == 0)
        {
            serial_print(" - OK (inode ");
            serial_print_hex(inode);
            serial_print(")\n");
        }
        else
        {
            serial_print(" - FAILED\n");
        }
    }
    serial_print("\n");

    // Test 3: List directory with files
    serial_print("[Test 3] Listing directory with files:\n");
    simplefs_list_dir(0);
    serial_print("\n");

    // Test 4: Write data to files
    serial_print("[Test 4] Writing data to files...\n");

    const char *test_data[] = {
        "Hello, World!",
        "This is a README file",
        "CONFIG DATA",
        "Boot configuration",
        "Kernel binary data",
        "Log entry 1\nLog entry 2",
        "Binary: 0xDEADBEEF",
        "Notes:\n- Item 1\n- Item 2",
        "System Config:\nVersion=1.0",
        "Temporary data"};

    for (int i = 0; i < 10; i++)
    {
        uint32_t file_inode;
        if (simplefs_find_file(0, test_files[i], &file_inode) == 0)
        {
            serial_print("  Writing to ");
            serial_print(test_files[i]);
            serial_print(" (inode ");
            serial_print_hex(file_inode);
            serial_print("): ");

            uint32_t len = 0;
            while (test_data[i][len])
                len++;

            int result = simplefs_write_file(file_inode, (const uint8_t *)test_data[i], len, 0);
            if (result >= 0)
            {
                serial_print("OK (");
                serial_print_hex(result);
                serial_print(" bytes)\n");
            }
            else
            {
                serial_print("FAILED\n");
            }
        }
    }
    serial_print("\n");

    // Test 5: Read data from files
    serial_print("[Test 5] Reading data from files...\n");

    uint8_t read_buffer[256];
    for (int i = 0; i < 10; i++)
    {
        uint32_t file_inode;
        if (simplefs_find_file(0, test_files[i], &file_inode) == 0)
        {
            serial_print("  Reading ");
            serial_print(test_files[i]);
            serial_print(": ");

            for (int j = 0; j < 256; j++)
                read_buffer[j] = 0;

            int bytes_read = simplefs_read_file(file_inode, read_buffer, 256, 0);
            if (bytes_read > 0)
            {
                serial_print("\"");
                for (int j = 0; j < bytes_read && j < 50; j++)
                {
                    if (read_buffer[j] >= 32 && read_buffer[j] < 127)
                        serial_putchar(read_buffer[j]);
                    else
                        serial_putchar('.');
                }
                if (bytes_read > 50)
                    serial_print("...");
                serial_print("\"\n");
            }
            else
            {
                serial_print("FAILED\n");
            }
        }
    }
    serial_print("\n");

    // Test 6: Find specific files
    serial_print("[Test 6] Finding specific files...\n");

    const char *search_files[] = {"hello.txt", "nonexistent.txt", "kernel.bin"};
    for (int i = 0; i < 3; i++)
    {
        serial_print("  Looking for: ");
        serial_print(search_files[i]);
        serial_print(" - ");

        uint32_t found_inode;
        if (simplefs_find_file(0, search_files[i], &found_inode) == 0)
        {
            serial_print("FOUND (inode ");
            serial_print_hex(found_inode);
            serial_print(")\n");
        }
        else
        {
            serial_print("NOT FOUND\n");
        }
    }
    serial_print("\n");

    // Test 7: Delete some files
    serial_print("[Test 7] Deleting files...\n");

    const char *delete_files[] = {"temp.tmp", "test.log", "data.dat"};
    for (int i = 0; i < 3; i++)
    {
        serial_print("  Deleting: ");
        serial_print(delete_files[i]);
        serial_print(" - ");

        if (simplefs_delete_file(0, delete_files[i]) == 0)
        {
            serial_print("OK\n");
        }
        else
        {
            serial_print("FAILED\n");
        }
    }
    serial_print("\n");

    // Test 8: List directory after deletions
    serial_print("[Test 8] Listing directory after deletions:\n");
    simplefs_list_dir(0);
    serial_print("\n");

    // Test 9: Verify deleted files are gone
    serial_print("[Test 9] Verifying deleted files...\n");
    for (int i = 0; i < 3; i++)
    {
        serial_print("  Checking: ");
        serial_print(delete_files[i]);
        serial_print(" - ");

        uint32_t found_inode;
        if (simplefs_find_file(0, delete_files[i], &found_inode) == 0)
        {
            serial_print("ERROR: Still exists!\n");
        }
        else
        {
            serial_print("Correctly deleted\n");
        }
    }
    serial_print("\n");

    // Test 10: Heap statistics
    serial_print("[Test 10] Heap statistics:\n");
    uint32_t total, used, free;
    heap_stats(&total, &used, &free);
    serial_print("  Total: ");
    serial_print_hex(total);
    serial_print(" bytes (");
    serial_print_hex(total / 1024);
    serial_print(" KB)\n");
    serial_print("  Used:  ");
    serial_print_hex(used);
    serial_print(" bytes (");
    serial_print_hex(used / 1024);
    serial_print(" KB)\n");
    serial_print("  Free:  ");
    serial_print_hex(free);
    serial_print(" bytes (");
    serial_print_hex(free / 1024);
    serial_print(" KB)\n");
    serial_print("\n");

    serial_print("=== Filesystem Test Complete ===\n\n");
}

// VFS wrapper functions
static struct vfs_node *simplefs_vfs_readdir(struct vfs_node *node, uint32_t index)
{
    if (!node || !(node->flags & VFS_DIRECTORY))
        return NULL;

    if (!simplefs_fs || !simplefs_fs->device)
        return NULL;

    if (index >= ROOT_DIR_ENTRIES)
        return NULL;

    uint32_t dir_size = sizeof(simplefs_dir_entry_t) * ROOT_DIR_ENTRIES;
    simplefs_dir_entry_t *entries = (simplefs_dir_entry_t *)kmalloc(dir_size);
    if (!entries)
        return NULL;

    // Read ALL directory blocks
    uint32_t blocks_needed = (dir_size + simplefs_fs->superblock.block_size - 1) / simplefs_fs->superblock.block_size;
    for (uint32_t i = 0; i < blocks_needed; i++)
    {
        uint8_t *block_ptr = ((uint8_t *)entries) + i * simplefs_fs->superblock.block_size;
        simplefs_fs->device->read_block(simplefs_fs->device,
                                        simplefs_fs->superblock.first_data_block + i,
                                        block_ptr);
    }

    // Count valid entries to know when to stop
    uint32_t count = 0;
    for (int i = 0; i < ROOT_DIR_ENTRIES; i++)
    {
        if (entries[i].inode_number != 0xFFFFFFFF && entries[i].inode_number < 256)
        {
            if (count == index)
            {
                // Found the entry at the requested index
                struct vfs_node *child = (struct vfs_node *)kmalloc(sizeof(struct vfs_node));
                if (!child)
                {
                    kfree(entries);
                    return NULL;
                }

                // ... rest of your existing code to populate child ...

                kfree(entries);
                return child;
            }
            count++;
        }
    }

    // Index is beyond available entries
    kfree(entries);
    return NULL;
}

static struct vfs_node *simplefs_vfs_finddir(struct vfs_node *node, const char *name)
{
    if (!node || !(node->flags & VFS_DIRECTORY) || !name)
        return NULL;

    if (!simplefs_fs || !simplefs_fs->device)
        return NULL;

    simplefs_dir_entry_t *entries = (simplefs_dir_entry_t *)kmalloc(512);
    if (!entries)
        return NULL;

    int result = simplefs_fs->device->read_block(
        simplefs_fs->device,
        simplefs_fs->superblock.first_data_block,
        (uint8_t *)entries);

    if (result != 0)
    {
        kfree(entries);
        return NULL;
    }

    for (int i = 0; i < 64; i++)
    {
        if (entries[i].inode_number != 0xFFFFFFFF && entries[i].inode_number < 256)
        {
            if (strcmp(entries[i].name, name) == 0)
            {
                struct vfs_node *child = (struct vfs_node *)kmalloc(sizeof(struct vfs_node));
                if (!child)
                {
                    kfree(entries);
                    return NULL;
                }

                // Zero and copy name
                for (uint32_t j = 0; j < sizeof(struct vfs_node); j++)
                    ((uint8_t *)child)[j] = 0;

                int name_len = entries[i].name_length;
                if (name_len > 255)
                    name_len = 255;
                for (int j = 0; j < name_len; j++)
                    child->name[j] = entries[i].name[j];
                child->name[name_len] = '\0';

                child->inode = entries[i].inode_number;
                child->flags = (entries[i].file_type == 2) ? VFS_DIRECTORY : VFS_FILE;
                child->length = 0;
                child->impl = 0;
                child->parent = node;
                child->filesystem = node->filesystem;
                child->mountpoint = NULL;

                // Set function pointers
                if (child->flags & VFS_DIRECTORY)
                {
                    child->read = NULL;
                    child->write = NULL;
                    child->readdir = simplefs_vfs_readdir;
                    child->finddir = simplefs_vfs_finddir;
                    child->create = simplefs_vfs_create;
                    child->delete = simplefs_vfs_delete;
                }
                else
                {
                    child->read = simplefs_vfs_read;
                    child->write = simplefs_vfs_write;
                    child->readdir = NULL;
                    child->finddir = NULL;
                    child->create = NULL;
                    child->delete = NULL;
                }
                child->open = simplefs_vfs_open;
                child->close = simplefs_vfs_close;

                kfree(entries);
                return child;
            }
        }
    }

    kfree(entries);
    return NULL;
}

int simplefs_find_file(uint32_t dir_inode_number, const char *filename, uint32_t *out_inode_number)
{
    if (!simplefs_fs || !simplefs_fs->device || !filename || !out_inode_number)
        return -1;

    simplefs_dir_entry_t *entries = (simplefs_dir_entry_t *)kmalloc(512);
    if (!entries)
        return -1;

    simplefs_fs->device->read_block(simplefs_fs->device, simplefs_fs->superblock.first_data_block, (uint8_t *)entries);

    for (int i = 0; i < 64; i++)
    {
        if (entries[i].inode_number != 0xFFFFFFFF && strcmp(entries[i].name, filename) == 0)
        {
            *out_inode_number = entries[i].inode_number;
            kfree(entries);
            return 0;
        }
    }

    kfree(entries);
    return -1;
}

static int simplefs_vfs_read(struct vfs_node *node, uint32_t offset, uint32_t size, uint8_t *buffer)
{
    if (!node || !buffer || (node->flags & VFS_DIRECTORY))
        return -1;

    return simplefs_read_file(node->inode, buffer, size, offset);
}

static int simplefs_vfs_write(struct vfs_node *node, uint32_t offset, uint32_t size, uint8_t *buffer)
{
    if (!node || !buffer || (node->flags & VFS_DIRECTORY))
        return -1;

    return simplefs_write_file(node->inode, buffer, size, offset);
}

static void simplefs_vfs_open(struct vfs_node *node, uint32_t flags)
{
    // No-op for SimpleFS
    (void)node;
    (void)flags;
}

static void simplefs_vfs_close(struct vfs_node *node)
{
    // No-op for SimpleFS
    (void)node;
}

static int simplefs_vfs_create(struct vfs_node *node, const char *name, uint32_t flags)
{
    if (!node || !name || !(node->flags & VFS_DIRECTORY))
        return -1;

    uint32_t new_inode;
    return simplefs_create_file(node->inode, name, &new_inode);
}

static int simplefs_vfs_delete(struct vfs_node *node, const char *name)
{
    if (!node || !name || !(node->flags & VFS_DIRECTORY))
        return -1;

    return simplefs_delete_file(node->inode, name);
}