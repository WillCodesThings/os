#include <fs/simplefs.h>
#include <disk/block_device.h>
#include <fs/vfs.h>
#include <memory/heap.h>
#include <shell/print.h>
#include <utils/string.h>

simplefs_superblock_t simplefs_superblock;
simplefs_header_t *simplefs_header = NULL;
vfs_node_t *simplefs_root = NULL;
simplefs_filesystem_t *simplefs_fs = NULL;

static block_device_t *simplefs_block_device = NULL;

struct vfs_node *simplefs_vfs_readdir(struct vfs_node *node, uint32_t index);
struct vfs_node *simplefs_vfs_finddir(struct vfs_node *node, const char *name);

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
    
    // Set function pointers for root directory
    simplefs_root->read = NULL;           // Can't read a directory as a file
    simplefs_root->write = NULL;          // Can't write to a directory as a file
    simplefs_root->open = NULL;           // Optional
    simplefs_root->close = NULL;          // Optional
    simplefs_root->readdir = simplefs_vfs_readdir;  // ← Add this
    simplefs_root->finddir = simplefs_vfs_finddir;  // ← Add this
    simplefs_root->create = NULL;         // TODO: implement later
    simplefs_root->delete = NULL;         // TODO: implement later


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

    // Write superblock to block 0
    block_device->write_block(block_device, 0, (uint8_t *)&sb);

    // **NEW: Initialize empty directory entries**
    simplefs_dir_entry_t empty_entries[64];
    for (int i = 0; i < 64; i++)
    {
        empty_entries[i].inode_number = 0xFFFFFFFF;  // Mark as empty
        empty_entries[i].name[0] = '\0';
        empty_entries[i].name_length = 0;
        empty_entries[i].file_type = 0;
    }
    
    // Write empty directory to first data block
    block_device->write_block(block_device, reserved_blocks, (uint8_t *)empty_entries);

    serial_print("SimpleFS formatted.\n");
}

void kernel_test_filesystem(void)
{
    serial_print("\n=== Testing Filesystem ===\n");
    
    serial_print("Step 1: Check simplefs_fs...\n");
    if (!simplefs_fs)
    {
        serial_print("ERROR: simplefs_fs is NULL!\n");
        return;
    }
    serial_print("simplefs_fs OK\n");
    
    serial_print("Step 2: Check device...\n");
    if (!simplefs_fs->device)
    {
        serial_print("ERROR: simplefs_fs->device is NULL!\n");
        return;
    }
    serial_print("Device OK\n");
    
    serial_print("Step 3: Creating file...\n");
    uint32_t test_inode;
    if (simplefs_create_file(0, "test.txt", &test_inode) == 0)
    {
        serial_print("Created test.txt with inode ");
        serial_print_hex(test_inode);
        serial_print("\n");
        
        serial_print("Step 4: Writing file...\n");
        const char *test_data = "Hello from SimpleFS with partitions!\n";
        uint32_t data_len = strlen(test_data);
        
        if (simplefs_write_file(test_inode, (uint8_t *)test_data, data_len, 0) > 0)
        {
            serial_print("Write successful\n");
            
            serial_print("Step 5: Reading file...\n");
            uint8_t read_buffer[512] = {0};
            if (simplefs_read_file(test_inode, read_buffer, 512, 0) > 0)
            {
                serial_print("Read successful: ");
                serial_print((char *)read_buffer);
            }
            else
            {
                serial_print("Read failed\n");
            }
        }
        else
        {
            serial_print("Write failed\n");
        }
    }
    else
    {
        serial_print("Create file failed\n");
    }
    
    serial_print("\nStep 6: Listing directory...\n");
    simplefs_list_dir(0);
    
    serial_print("\n=== Filesystem Test Complete ===\n");
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

    simplefs_superblock_t sb;
    int result = block_device->read_block(block_device, 0, (uint8_t *)&sb);
    
    if (result != 0)
    {
        serial_print("Failed to read superblock\n");
        return -1;
    }

    serial_print("Magic: ");
    serial_print_hex(sb.header->magic);
    serial_print("\n");

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
    
    // Make SURE the device is set
    simplefs_fs->device = block_device;

    serial_print("Mounted SimpleFS v");
    serial_print_hex(sb.header->version);
    serial_print("\n");

    return 0;
}

// void simplefs_test_setup(void)
// {
//     simplefs_write_file(1, (uint8_t *)"Hello, world!\n", 13, 0);
//     simplefs_write_file(2, (uint8_t *)"This is a test file.\n", 19, 0);
//     simplefs_write_file(3, (uint8_t *)"Test file in docs directory\n", 29, 0);

//     simplefs_list_dir(1);
//     simplefs_list_dir(2);
//     simplefs_list_dir(3);
// }

int simplefs_read_block(uint32_t block_number, void *buffer)
{
    if (!simplefs_block_device || !buffer)
        return -1;

    return simplefs_fs->device->read_block(simplefs_block_device, block_number, (uint8_t *)buffer);
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

struct vfs_node *simplefs_vfs_readdir(struct vfs_node *node, uint32_t index)
{
    if (!node)
    {
        serial_print("readdir: NULL node\n");
        return NULL;
    }
    
    if (!(node->flags & VFS_DIRECTORY))
    {
        serial_print("readdir: Not a directory\n");
        return NULL;
    }
    
    if (!simplefs_fs)
    {
        serial_print("readdir: simplefs_fs is NULL\n");
        return NULL;
    }
    
    if (!simplefs_fs->device)
    {
        serial_print("readdir: device is NULL\n");
        return NULL;
    }
    
    // Limit index to prevent runaway
    if (index >= 64)
    {
        serial_print("readdir: index too large\n");
        return NULL;
    }
    
    simplefs_dir_entry_t entries[64];  // Use stack instead of heap
    
    serial_print("readdir: reading block ");
    serial_print_hex(simplefs_fs->superblock.first_data_block);
    serial_print("\n");
    
    int result = simplefs_fs->device->read_block(
        simplefs_fs->device,
        simplefs_fs->superblock.first_data_block,
        (uint8_t *)entries
    );
    
    if (result != 0)
    {
        serial_print("readdir: read failed\n");
        return NULL;
    }
    
    uint32_t count = 0;
    for (int i = 0; i < 64; i++)
    {
        if (entries[i].inode_number != 0xFFFFFFFF && entries[i].inode_number < 256)
        {
            if (count == index)
            {
                serial_print("readdir: found entry ");
                serial_print_hex(i);
                serial_print("\n");
                
                struct vfs_node *child = (struct vfs_node *)kmalloc(sizeof(struct vfs_node));
                if (!child)
                {
                    serial_print("readdir: kmalloc failed\n");
                    return NULL;
                }
                
                // Zero out the structure first
                for (int j = 0; j < sizeof(struct vfs_node); j++)
                {
                    ((uint8_t *)child)[j] = 0;
                }
                
                int name_len = entries[i].name_length;
                if (name_len > 255) name_len = 255;
                
                for (int j = 0; j < name_len; j++)
                {
                    child->name[j] = entries[i].name[j];
                }
                child->name[name_len] = '\0';
                
                child->inode = entries[i].inode_number;
                child->flags = (entries[i].file_type == 2) ? VFS_DIRECTORY : 0;
                child->parent = node;
                child->filesystem = node->filesystem;
                
                serial_print("readdir: returning child\n");
                return child;
            }
            count++;
        }
    }
    
    serial_print("readdir: no more entries\n");
    return NULL;
}
struct vfs_node *simplefs_vfs_finddir(struct vfs_node *node, const char *name)
{
    if (!node || !(node->flags & VFS_DIRECTORY))
        return NULL;
    
    if (!simplefs_fs || !simplefs_fs->device)
        return NULL;
    
    simplefs_dir_entry_t entries[64];
    int result = simplefs_fs->device->read_block(
        simplefs_fs->device,
        simplefs_fs->superblock.first_data_block,
        (uint8_t *)entries
    );
    
    if (result != 0)
        return NULL;
    
    for (int i = 0; i < 64; i++)
    {
        if (entries[i].inode_number != 0xFFFFFFFF && entries[i].inode_number < 256)
        {
            if (strcmp(entries[i].name, name) == 0)
            {
                struct vfs_node *child = (struct vfs_node *)kmalloc(sizeof(struct vfs_node));
                if (!child)
                    return NULL;
                
                int name_len = entries[i].name_length;
                if (name_len > 255) name_len = 255;
                for (int j = 0; j < name_len; j++)
                {
                    child->name[j] = entries[i].name[j];
                }
                child->name[name_len] = '\0';
                
                child->inode = entries[i].inode_number;
                child->flags = (entries[i].file_type == 2) ? VFS_DIRECTORY : 0;
                child->length = 0;
                child->impl = 0;
                child->parent = node;
                child->filesystem = node->filesystem;
                child->mountpoint = NULL;
                child->read = NULL;
                child->write = NULL;
                child->open = NULL;
                child->close = NULL;
                child->readdir = NULL;
                child->finddir = NULL;
                child->create = NULL;
                child->delete = NULL;
                
                return child;
            }
        }
    }
    
    return NULL;
}