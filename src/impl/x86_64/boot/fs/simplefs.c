#include <fs/simplefs.h>
#include <disk/block_device.h>
#include <fs/vfs.h>
#include <fs/tmpfs.h>
#include <memory/heap.h>
#include <shell/print.h>
#include <utils/string.h>

simplefs_superblock_t simplefs_superblock;
simplefs_header_t *simplefs_header = NULL;
vfs_node_t *simplefs_root = NULL;
simplefs_filesystem_t *simplefs_fs = NULL;

static block_device_t *simplefs_block_device = NULL;

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
    simplefs_root->parent = NULL;

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

    simplefs_header = (simplefs_header_t *)kmalloc(sizeof(simplefs_header_t));
    simplefs_header->version = 1;
    simplefs_header->magic = 0x53465321;

    // Mount the filesystem
    vfs_set_root(simplefs_root);
    simplefs_block_device = block_device;

    serial_print("SimpleFS initialized.\n");
}

void simplefs_format(block_device_t *block_device, uint32_t total_blocks, int reserved_blocks)
{
    simplefs_superblock_t sb;
    serial_print("Formatting SimpleFS...\n");
    
    sb.header = simplefs_header;
    sb.block_size = 4096;
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

    // Write superblock to block 0
    block_device->write_block(block_device, 0, (uint8_t *)&sb);

    serial_print("SimpleFS formatted.\n");
}

int simplefs_mount(block_device_t *block_device)
{
    simplefs_block_device = block_device;

    simplefs_superblock_t sb;
    block_device->read_block(block_device, 0, (uint8_t *)&sb);

    if (sb.header->magic != 0x53465321)
    {
        serial_print("Invalid filesystem magic!\n");
        return -1;
    }

    if (sb.header->version > 1)
    {
        serial_print("Unsupported filesystem version\n");
        return -1;
    }

    sb.max_inode_count = sb.inodetable_start * (sb.block_size / sizeof(simplefs_inode_t));
    // Cache superblock
    simplefs_superblock = sb;
    simplefs_fs->superblock = sb;

    serial_print("Mounted SimpleFS v");
    print_int(sb.header->version);
    serial_print("\n");

    return 0;
}

void simplefs_test_setup(void)
{
    simplefs_write_file(1, (uint8_t *)"Hello, world!\n", 13, 0);
    simplefs_write_file(2, (uint8_t *)"This is a test file.\n", 19, 0);
    simplefs_write_file(3, (uint8_t *)"Test file in docs directory\n", 29, 0);

    simplefs_list_dir(1);
    simplefs_list_dir(2);
    simplefs_list_dir(3);
}

int simplefs_read_block(uint32_t block_number, void *buffer)
{
    if (!simplefs_block_device || !buffer)
        return -1;

    return simplefs_block_device->read_block(simplefs_block_device, block_number, (uint8_t *)buffer);
}

int simplefs_read_file(uint32_t inode_number, uint8_t *buffer, uint32_t size, uint32_t offset)
{
    simplefs_inode_t inode;
    simplefs_fs->device->read_block(simplefs_fs->device, simplefs_fs->superblock.inodetable_start + inode_number, (uint8_t *)&inode);

    if (inode.file_size == 0 || size == 0)
        return 0;

    uint32_t block_num = inode.direct_blocks[0]; // single-block file for now
    simplefs_fs->device->read_block(simplefs_fs->device, block_num, buffer);

    return inode.file_size;
}

int simplefs_write_file(uint32_t inode_number, const uint8_t *buffer, uint32_t size, uint32_t offset)
{
    simplefs_inode_t inode;
    simplefs_fs->device->read_block(simplefs_fs->device, simplefs_fs->superblock.inodetable_start + inode_number, (uint8_t *)&inode);

    if (inode.direct_blocks[0] == 0)
    {
        // Allocate first block
        inode.direct_blocks[0] = simplefs_fs->superblock.first_data_block + inode_number;
    }

    simplefs_fs->device->write_block(simplefs_fs->device, inode.direct_blocks[0], buffer);

    inode.file_size = size;
    simplefs_fs->device->write_block(simplefs_fs->device, simplefs_fs->superblock.inodetable_start + inode_number, (uint8_t *)&inode);

    return size;
}

int simplefs_list_dir(uint32_t dir_inode_number)
{
    if (!simplefs_fs->device)
        return -1;

    // Read root directory block
    simplefs_dir_entry_t entries[64]; // assume 64 entries max per block
    simplefs_fs->device->read_block(simplefs_fs->device, simplefs_fs->superblock.first_data_block, (uint8_t *)entries);

    serial_print("Directory listing:\n");
    for (int i = 0; i < 64; i++)
    {
        if (entries[i].inode_number != 0xFFFFFFFF) // unused entry
        {
            serial_print(" - ");
            serial_print(entries[i].name);
            serial_print("\n");
        }
    }

    return 0;
}

int simplefs_find_file(uint32_t dir_inode_number, const char *filename, uint32_t *out_inode_number)
{
    if (!simplefs_fs->device || !filename || !out_inode_number)
        return -1;

    simplefs_dir_entry_t entries[64];
    simplefs_fs->device->read_block(simplefs_fs->device, simplefs_fs->superblock.first_data_block, (uint8_t *)entries);

    for (int i = 0; i < 64; i++)
    {
        if (entries[i].inode_number != 0xFFFFFFFF && strcmp(entries[i].name, filename) == 0)
        {
            *out_inode_number = entries[i].inode_number;
            return 0;
        }
    }

    return -1; // not found
}

int simplefs_create_file(uint32_t dir_inode_number, const char *filename, uint32_t *out_inode)
{
    if (simplefs_fs->superblock.free_inode_count == 0)
        return -1;

    uint32_t inode_number = simplefs_fs->superblock.max_inode_count - simplefs_fs->superblock.free_inode_count;
    simplefs_fs->superblock.free_inode_count--;

    simplefs_inode_t new_inode = {0};
    simplefs_fs->device->write_block(simplefs_fs->device, simplefs_fs->superblock.inodetable_start + inode_number, (uint8_t *)&new_inode);

    // Add to root directory block (very simplified)
    simplefs_dir_entry_t entries[64];
    simplefs_fs->device->read_block(simplefs_fs->device, simplefs_fs->superblock.first_data_block, (uint8_t *)entries);

    simplefs_dir_entry_t entry;
    entry.inode_number = inode_number;
    entry.name_length = strlen(filename);
    entry.file_type = 1; // file
    strcpy(entry.name, filename);

    entries[inode_number % 64] = entry;
    simplefs_fs->device->write_block(simplefs_fs->device, simplefs_fs->superblock.first_data_block, (uint8_t *)entries);

    *out_inode = inode_number;
    return 0;
}

int simplefs_delete_file(uint32_t dir_inode_number, const char *filename)
{
    if (!simplefs_fs->device || !filename)
        return -1;

    uint32_t inode_number;
    if (simplefs_find_file(dir_inode_number, filename, &inode_number) != 0)
        return -1; // file doesn't exist

    // Clear inode
    simplefs_inode_t empty_inode = {0};
    simplefs_fs->device->write_block(simplefs_fs->device, simplefs_fs->superblock.inodetable_start + inode_number, (uint8_t *)&empty_inode);

    // Clear directory entry
    simplefs_dir_entry_t entries[64];
    simplefs_fs->device->read_block(simplefs_fs->device, simplefs_fs->superblock.first_data_block, (uint8_t *)entries);

    for (int i = 0; i < 64; i++)
    {
        if (entries[i].inode_number == inode_number)
        {
            entries[i].inode_number = 0xFFFFFFFF; // mark empty
            entries[i].name[0] = '\0';
            break;
        }
    }

    simplefs_fs->device->write_block(simplefs_fs->device, simplefs_fs->superblock.first_data_block, (uint8_t *)entries);

    simplefs_fs->superblock.free_inode_count++; // increment free inode count

    return 0;
}
