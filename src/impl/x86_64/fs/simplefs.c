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

    // Allocate superblock on heap to avoid stack overflow
    simplefs_superblock_t *sb = (simplefs_superblock_t *)kmalloc(sizeof(simplefs_superblock_t));
    if (!sb)
    {
        serial_print("ERROR: Failed to allocate superblock\n");
        return;
    }

    // Zero and setup superblock
    for (uint32_t i = 0; i < sizeof(simplefs_superblock_t); i++)
        ((uint8_t *)sb)[i] = 0;

    sb->header.magic = SIMPLEFS_MAGIC;
    sb->header.version = 1;
    sb->block_size = block_device->block_size;
    sb->total_blocks = total_blocks;
    sb->free_block_count = sb->total_blocks - reserved_blocks;
    sb->first_data_block = reserved_blocks;
    sb->inodetable_start = 1;
    sb->inodetable_blocks = 10;
    // Calculate actual max inode count based on inode table size
    uint32_t max_inodes = sb->inodetable_blocks * (sb->block_size / sizeof(simplefs_inode_t));
    sb->inode_count = max_inodes;
    sb->free_inode_count = max_inodes;
    sb->max_inode_count = max_inodes;
    sb->mount_count = 0;
    sb->last_mount_time = 0;
    sb->last_write_time = 0;

    // Write superblock to block 0
    int result = block_device->write_block(block_device, 0, (uint8_t *)sb);
    if (result != 0)
    {
        serial_print("ERROR: Failed to write superblock\n");
        kfree(sb);
        return;
    }

    // Verify superblock was written correctly (reuse same buffer)
    for (uint32_t i = 0; i < sizeof(simplefs_superblock_t); i++)
        ((uint8_t *)sb)[i] = 0;

    result = block_device->read_block(block_device, 0, (uint8_t *)sb);
    if (result != 0)
    {
        serial_print("ERROR: Failed to read back superblock\n");
        kfree(sb);
        return;
    }

    if (sb->header.magic != SIMPLEFS_MAGIC)
    {
        serial_print("ERROR: Superblock verification failed!\n");
        kfree(sb);
        return;
    }

    kfree(sb);

    // --- Write empty root directory blocks one at a time ---
    // This avoids large allocations that might cause issues
    uint32_t entries_per_block = block_device->block_size / sizeof(simplefs_dir_entry_t);
    if (entries_per_block == 0)
        entries_per_block = 1; // At least 1 entry per block
    uint32_t blocks_needed = (ROOT_DIR_ENTRIES + entries_per_block - 1) / entries_per_block;

    // Allocate single block buffer
    uint8_t *block_buffer = (uint8_t *)kmalloc(block_device->block_size);
    if (!block_buffer)
    {
        serial_print("Failed to allocate block buffer\n");
        return;
    }

    serial_print("Writing directory blocks...\n");

    uint32_t entry_idx = 0;
    for (uint32_t block = 0; block < blocks_needed; block++)
    {
        // Zero the block
        for (uint32_t i = 0; i < block_device->block_size; i++)
            block_buffer[i] = 0;

        // Fill with empty directory entries
        simplefs_dir_entry_t *entries = (simplefs_dir_entry_t *)block_buffer;
        for (uint32_t e = 0; e < entries_per_block && entry_idx < ROOT_DIR_ENTRIES; e++, entry_idx++)
        {
            entries[e].inode_number = 0xFFFFFFFF;
            entries[e].name[0] = '\0';
            entries[e].name_length = 0;
            entries[e].file_type = 0;
            entries[e].record_length = sizeof(simplefs_dir_entry_t);
        }

        // Write block to disk
        int write_result = block_device->write_block(block_device, reserved_blocks + block, block_buffer);
        if (write_result != 0)
        {
            serial_print("ERROR: Failed to write directory block\n");
            kfree(block_buffer);
            return;
        }
    }

    kfree(block_buffer);
    serial_print("SimpleFS formatted successfully.\n");
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
    (void)offset;

    if (!simplefs_fs || !simplefs_fs->device || !buffer)
        return -1;

    // Use a proper 512-byte buffer for block I/O
    uint8_t *inode_buf = (uint8_t *)kmalloc(512);
    if (!inode_buf)
        return -1;

    simplefs_fs->device->read_block(simplefs_fs->device,
                                    simplefs_fs->superblock.inodetable_start + inode_number,
                                    inode_buf);

    simplefs_inode_t *inode = (simplefs_inode_t *)inode_buf;

    if (inode->file_size == 0 || size == 0)
    {
        kfree(inode_buf);
        return 0;
    }

    uint32_t block_num = inode->direct_blocks[0];
    uint32_t file_size = inode->file_size;
    kfree(inode_buf);

    // Use a 512-byte buffer for block read, then copy to user buffer
    uint8_t *data_buf = (uint8_t *)kmalloc(512);
    if (!data_buf)
        return -1;

    simplefs_fs->device->read_block(simplefs_fs->device, block_num, data_buf);

    // Copy only the requested amount to user buffer
    uint32_t bytes_to_copy = (file_size < size) ? file_size : size;
    for (uint32_t i = 0; i < bytes_to_copy; i++)
        buffer[i] = data_buf[i];

    kfree(data_buf);

    return bytes_to_copy;
}

int simplefs_write_file(uint32_t inode_number, const uint8_t *buffer, uint32_t size, uint32_t offset)
{
    (void)offset;

    if (!simplefs_fs || !simplefs_fs->device || !buffer)
        return -1;

    // Use a proper 512-byte buffer for block I/O
    uint8_t *inode_buf = (uint8_t *)kmalloc(512);
    if (!inode_buf)
        return -1;

    // Read inode block
    simplefs_fs->device->read_block(simplefs_fs->device,
                                    simplefs_fs->superblock.inodetable_start + inode_number,
                                    inode_buf);

    simplefs_inode_t *inode = (simplefs_inode_t *)inode_buf;

    if (inode->direct_blocks[0] == 0)
    {
        // Allocate first data block for this file
        // Use a simple allocation: first_data_block + dir_blocks + inode_number
        // Directory takes blocks first_data_block to first_data_block + 63
        // File data starts at first_data_block + 64 + inode_number
        inode->direct_blocks[0] = simplefs_fs->superblock.first_data_block + 64 + inode_number;
    }

    // Write data (need a 512-byte buffer)
    uint8_t *data_buf = (uint8_t *)kmalloc(512);
    if (!data_buf)
    {
        kfree(inode_buf);
        return -1;
    }

    // Zero and copy data
    for (int i = 0; i < 512; i++)
        data_buf[i] = 0;
    for (uint32_t i = 0; i < size && i < 512; i++)
        data_buf[i] = buffer[i];

    simplefs_fs->device->write_block(simplefs_fs->device, inode->direct_blocks[0], data_buf);
    kfree(data_buf);

    // Update inode size and write back
    inode->file_size = size;
    simplefs_fs->device->write_block(simplefs_fs->device,
                                     simplefs_fs->superblock.inodetable_start + inode_number,
                                     inode_buf);
    kfree(inode_buf);

    return size;
}

int simplefs_list_dir(uint32_t dir_inode_number)
{
    (void)dir_inode_number; // Currently only supports root directory

    if (!simplefs_fs || !simplefs_fs->device)
        return -1;

    uint8_t *block_buffer = (uint8_t *)kmalloc(512);
    if (!block_buffer)
        return -1;

    serial_print("Directory listing:\n");
    int count = 0;

    uint32_t entries_per_block = 512 / sizeof(simplefs_dir_entry_t);
    if (entries_per_block == 0)
        entries_per_block = 1;
    uint32_t dir_blocks = (ROOT_DIR_ENTRIES + entries_per_block - 1) / entries_per_block;

    for (uint32_t blk = 0; blk < dir_blocks; blk++)
    {
        simplefs_fs->device->read_block(simplefs_fs->device,
                                        simplefs_fs->superblock.first_data_block + blk,
                                        block_buffer);

        simplefs_dir_entry_t *entries = (simplefs_dir_entry_t *)block_buffer;
        uint32_t entries_in_block = entries_per_block;
        uint32_t remaining = ROOT_DIR_ENTRIES - blk * entries_per_block;
        if (remaining < entries_in_block)
            entries_in_block = remaining;

        for (uint32_t e = 0; e < entries_in_block; e++)
        {
            if (entries[e].inode_number != 0xFFFFFFFF && entries[e].inode_number < 256)
            {
                serial_print(" - ");
                serial_print(entries[e].name);
                serial_print("\n");
                count++;
            }
        }
    }

    if (count == 0)
    {
        serial_print(" (empty)\n");
    }

    kfree(block_buffer);
    return 0;
}

int simplefs_create_file(uint32_t dir_inode_number, const char *filename, uint32_t *out_inode)
{
    (void)dir_inode_number; // Currently only supports root directory

    if (!simplefs_fs || !filename || !out_inode)
        return -1;

    if (simplefs_fs->superblock.free_inode_count == 0)
        return -1;

    uint32_t inode_number = simplefs_fs->superblock.max_inode_count - simplefs_fs->superblock.free_inode_count;
    simplefs_fs->superblock.free_inode_count--;

    // Write empty inode (need full 512-byte buffer for block write)
    uint8_t *inode_buffer = (uint8_t *)kmalloc(512);
    if (!inode_buffer)
        return -1;

    for (int i = 0; i < 512; i++)
        inode_buffer[i] = 0;

    simplefs_fs->device->write_block(simplefs_fs->device,
                                     simplefs_fs->superblock.inodetable_start + inode_number,
                                     inode_buffer);
    kfree(inode_buffer);

    // Find empty slot in directory blocks
    uint8_t *block_buffer = (uint8_t *)kmalloc(512);
    if (!block_buffer)
        return -1;

    uint32_t entries_per_block = 512 / sizeof(simplefs_dir_entry_t);
    if (entries_per_block == 0)
        entries_per_block = 1;
    uint32_t dir_blocks = (ROOT_DIR_ENTRIES + entries_per_block - 1) / entries_per_block;
    uint32_t entry_idx = 0;

    for (uint32_t blk = 0; blk < dir_blocks; blk++)
    {
        if (simplefs_fs->device->read_block(simplefs_fs->device,
                                            simplefs_fs->superblock.first_data_block + blk,
                                            block_buffer) != 0)
        {
            kfree(block_buffer);
            return -1;
        }

        simplefs_dir_entry_t *entries = (simplefs_dir_entry_t *)block_buffer;
        uint32_t entries_in_block = entries_per_block;
        uint32_t remaining = ROOT_DIR_ENTRIES - blk * entries_per_block;
        if (remaining < entries_in_block)
            entries_in_block = remaining;

        for (uint32_t e = 0; e < entries_in_block; e++, entry_idx++)
        {
            if (entries[e].inode_number == 0xFFFFFFFF)
            {
                // Found empty slot
                entries[e].inode_number = inode_number;
                uint8_t name_len = 0;
                while (filename[name_len] && name_len < 251)
                {
                    entries[e].name[name_len] = filename[name_len];
                    name_len++;
                }
                entries[e].name[name_len] = '\0';
                entries[e].name_length = name_len;
                entries[e].file_type = 1; // Regular file
                entries[e].record_length = sizeof(simplefs_dir_entry_t);

                // Write back the block
                simplefs_fs->device->write_block(simplefs_fs->device,
                                                 simplefs_fs->superblock.first_data_block + blk,
                                                 block_buffer);
                kfree(block_buffer);
                *out_inode = inode_number;
                return 0;
            }
        }
    }

    kfree(block_buffer);
    return -1; // No empty slot found
}

int simplefs_delete_file(uint32_t dir_inode_number, const char *filename)
{
    (void)dir_inode_number; // Currently only supports root directory

    if (!simplefs_fs || !simplefs_fs->device || !filename)
        return -1;

    uint32_t inode_number;
    if (simplefs_find_file(dir_inode_number, filename, &inode_number) != 0)
        return -1;

    // Clear inode (need full 512-byte buffer)
    uint8_t *inode_buffer = (uint8_t *)kmalloc(512);
    if (!inode_buffer)
        return -1;
    for (int i = 0; i < 512; i++)
        inode_buffer[i] = 0;
    simplefs_fs->device->write_block(simplefs_fs->device, simplefs_fs->superblock.inodetable_start + inode_number, inode_buffer);
    kfree(inode_buffer);

    // Find and clear directory entry
    uint8_t *block_buffer = (uint8_t *)kmalloc(512);
    if (!block_buffer)
        return -1;

    uint32_t entries_per_block = 512 / sizeof(simplefs_dir_entry_t);
    if (entries_per_block == 0)
        entries_per_block = 1;
    uint32_t dir_blocks = (ROOT_DIR_ENTRIES + entries_per_block - 1) / entries_per_block;

    for (uint32_t blk = 0; blk < dir_blocks; blk++)
    {
        simplefs_fs->device->read_block(simplefs_fs->device,
                                        simplefs_fs->superblock.first_data_block + blk,
                                        block_buffer);

        simplefs_dir_entry_t *entries = (simplefs_dir_entry_t *)block_buffer;
        uint32_t entries_in_block = entries_per_block;
        uint32_t remaining = ROOT_DIR_ENTRIES - blk * entries_per_block;
        if (remaining < entries_in_block)
            entries_in_block = remaining;

        for (uint32_t e = 0; e < entries_in_block; e++)
        {
            if (entries[e].inode_number == inode_number)
            {
                entries[e].inode_number = 0xFFFFFFFF;
                entries[e].name[0] = '\0';
                entries[e].name_length = 0;

                simplefs_fs->device->write_block(simplefs_fs->device,
                                                 simplefs_fs->superblock.first_data_block + blk,
                                                 block_buffer);
                kfree(block_buffer);
                simplefs_fs->superblock.free_inode_count++;
                return 0;
            }
        }
    }

    kfree(block_buffer);
    return -1; // Entry not found
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

// Create some sample files for testing
void simplefs_create_sample_files(void)
{
    if (!simplefs_fs || !simplefs_fs->device)
    {
        serial_print("Cannot create sample files: filesystem not mounted\n");
        return;
    }

    serial_print("Creating sample files...\n");

    // Create and write test files
    struct
    {
        const char *name;
        const char *content;
    } files[] = {
        {"hello.txt", "Hello, World!\nWelcome to SimpleFS!"},
        {"readme.txt", "This is a simple filesystem test.\nCreated by the kernel."},
        {"notes.txt", "TODO:\n- Fix bugs\n- Add features\n- Test more"}};

    for (int i = 0; i < 3; i++)
    {
        uint32_t inode;
        if (simplefs_create_file(0, files[i].name, &inode) == 0)
        {
            // Write content
            uint32_t len = 0;
            while (files[i].content[len])
                len++;
            simplefs_write_file(inode, (const uint8_t *)files[i].content, len, 0);
            serial_print("  Created: ");
            serial_print(files[i].name);
            serial_print("\n");
        }
    }

    serial_print("Sample files created.\n");

    // Verify by listing directory
    serial_print("Directory contents:\n");
    simplefs_list_dir(0);
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

    // Allocate single block buffer to avoid large allocations
    uint8_t *block_buffer = (uint8_t *)kmalloc(512);
    if (!block_buffer)
        return NULL;

    uint32_t entries_per_block = 512 / sizeof(simplefs_dir_entry_t);
    if (entries_per_block == 0)
        entries_per_block = 1;
    uint32_t blocks_needed = (ROOT_DIR_ENTRIES + entries_per_block - 1) / entries_per_block;

    // Count valid entries across all blocks
    uint32_t count = 0;
    for (uint32_t blk = 0; blk < blocks_needed; blk++)
    {
        int result = simplefs_fs->device->read_block(
            simplefs_fs->device,
            simplefs_fs->superblock.first_data_block + blk,
            block_buffer);

        if (result != 0)
        {
            kfree(block_buffer);
            return NULL;
        }

        simplefs_dir_entry_t *entries = (simplefs_dir_entry_t *)block_buffer;
        uint32_t entries_in_block = entries_per_block;

        // Last block might have fewer entries
        uint32_t remaining = ROOT_DIR_ENTRIES - blk * entries_per_block;
        if (remaining < entries_in_block)
            entries_in_block = remaining;

        for (uint32_t e = 0; e < entries_in_block; e++)
        {
            if (entries[e].inode_number != 0xFFFFFFFF && entries[e].inode_number < 256)
            {
                if (count == index)
                {
                    // Found the entry at the requested index
                    struct vfs_node *child = (struct vfs_node *)kmalloc(sizeof(struct vfs_node));
                    if (!child)
                    {
                        kfree(block_buffer);
                        return NULL;
                    }

                    // Zero the node
                    for (uint32_t j = 0; j < sizeof(struct vfs_node); j++)
                        ((uint8_t *)child)[j] = 0;

                    // Copy name
                    int name_len = entries[e].name_length;
                    if (name_len > 255)
                        name_len = 255;
                    for (int j = 0; j < name_len; j++)
                        child->name[j] = entries[e].name[j];
                    child->name[name_len] = '\0';

                    child->inode = entries[e].inode_number;
                    child->flags = (entries[e].file_type == 2) ? VFS_DIRECTORY : VFS_FILE;
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

                    kfree(block_buffer);
                    return child;
                }
                count++;
            }
        }
    }

    // Index is beyond available entries
    kfree(block_buffer);
    return NULL;
}

static struct vfs_node *simplefs_vfs_finddir(struct vfs_node *node, const char *name)
{
    if (!node || !(node->flags & VFS_DIRECTORY) || !name)
        return NULL;

    if (!simplefs_fs || !simplefs_fs->device)
        return NULL;

    uint8_t *block_buffer = (uint8_t *)kmalloc(512);
    if (!block_buffer)
        return NULL;

    uint32_t entries_per_block = 512 / sizeof(simplefs_dir_entry_t);
    if (entries_per_block == 0)
        entries_per_block = 1;
    uint32_t dir_blocks = (ROOT_DIR_ENTRIES + entries_per_block - 1) / entries_per_block;

    for (uint32_t blk = 0; blk < dir_blocks; blk++)
    {
        int result = simplefs_fs->device->read_block(
            simplefs_fs->device,
            simplefs_fs->superblock.first_data_block + blk,
            block_buffer);

        if (result != 0)
        {
            kfree(block_buffer);
            return NULL;
        }

        simplefs_dir_entry_t *entries = (simplefs_dir_entry_t *)block_buffer;
        uint32_t entries_in_block = entries_per_block;
        uint32_t remaining = ROOT_DIR_ENTRIES - blk * entries_per_block;
        if (remaining < entries_in_block)
            entries_in_block = remaining;

        for (uint32_t e = 0; e < entries_in_block; e++)
        {
            if (entries[e].inode_number != 0xFFFFFFFF && entries[e].inode_number < 256)
            {
                if (strcmp(entries[e].name, name) == 0)
                {
                    struct vfs_node *child = (struct vfs_node *)kmalloc(sizeof(struct vfs_node));
                    if (!child)
                    {
                        kfree(block_buffer);
                        return NULL;
                    }

                    // Zero and copy name
                    for (uint32_t j = 0; j < sizeof(struct vfs_node); j++)
                        ((uint8_t *)child)[j] = 0;

                    int name_len = entries[e].name_length;
                    if (name_len > 255)
                        name_len = 255;
                    for (int j = 0; j < name_len; j++)
                        child->name[j] = entries[e].name[j];
                    child->name[name_len] = '\0';

                    child->inode = entries[e].inode_number;
                    child->flags = (entries[e].file_type == 2) ? VFS_DIRECTORY : VFS_FILE;
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

                    kfree(block_buffer);
                    return child;
                }
            }
        }
    }

    kfree(block_buffer);
    return NULL;
}

int simplefs_find_file(uint32_t dir_inode_number, const char *filename, uint32_t *out_inode_number)
{
    (void)dir_inode_number; // Currently only supports root directory

    if (!simplefs_fs || !simplefs_fs->device || !filename || !out_inode_number)
        return -1;

    uint8_t *block_buffer = (uint8_t *)kmalloc(512);
    if (!block_buffer)
        return -1;

    uint32_t entries_per_block = 512 / sizeof(simplefs_dir_entry_t);
    if (entries_per_block == 0)
        entries_per_block = 1;
    uint32_t dir_blocks = (ROOT_DIR_ENTRIES + entries_per_block - 1) / entries_per_block;

    for (uint32_t blk = 0; blk < dir_blocks; blk++)
    {
        simplefs_fs->device->read_block(simplefs_fs->device,
                                        simplefs_fs->superblock.first_data_block + blk,
                                        block_buffer);

        simplefs_dir_entry_t *entries = (simplefs_dir_entry_t *)block_buffer;
        uint32_t entries_in_block = entries_per_block;
        uint32_t remaining = ROOT_DIR_ENTRIES - blk * entries_per_block;
        if (remaining < entries_in_block)
            entries_in_block = remaining;

        for (uint32_t e = 0; e < entries_in_block; e++)
        {
            if (entries[e].inode_number != 0xFFFFFFFF && strcmp(entries[e].name, filename) == 0)
            {
                *out_inode_number = entries[e].inode_number;
                kfree(block_buffer);
                return 0;
            }
        }
    }

    kfree(block_buffer);
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