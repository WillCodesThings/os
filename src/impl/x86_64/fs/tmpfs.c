#include <fs/tmpfs.h>
#include <fs/vfs.h>
#include <shell/shell.h>

#define MAX_FILES 256
#define MAX_FILE_SIZE 4096

typedef struct
{
    uint8_t data[MAX_FILE_SIZE];
    uint32_t size;
    vfs_node_t *children[MAX_FILES];
    uint32_t child_count;
} tmpfs_data_t;

static tmpfs_data_t file_data[MAX_FILES];
static uint32_t next_inode = 1;

// Allocate a new tmpfs data structure
static tmpfs_data_t *tmpfs_alloc_data(void)
{
    static int next_free = 0;
    if (next_free >= MAX_FILES)
        return NULL;

    tmpfs_data_t *data = &file_data[next_free++];
    data->size = 0;
    data->child_count = 0;

    return data;
}

// tmpfs read function
static int tmpfs_read(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer)
{
    if (!node || !buffer)
        return -1;

    tmpfs_data_t *data = (tmpfs_data_t *)node->filesystem;
    if (!data)
        return -1;

    if (offset >= data->size)
        return 0;

    if (offset + size > data->size)
    {
        size = data->size - offset;
    }

    for (uint32_t i = 0; i < size; i++)
    {
        buffer[i] = data->data[offset + i];
    }

    return size;
}

// tmpfs write function
static int tmpfs_write(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer)
{
    if (!node || !buffer)
        return -1;

    tmpfs_data_t *data = (tmpfs_data_t *)node->filesystem;
    if (!data)
        return -1;

    if (offset + size > MAX_FILE_SIZE)
    {
        size = MAX_FILE_SIZE - offset;
    }

    for (uint32_t i = 0; i < size; i++)
    {
        data->data[offset + i] = buffer[i];
    }

    if (offset + size > data->size)
    {
        data->size = offset + size;
        node->length = data->size;
    }

    return size;
}

// tmpfs readdir function
static vfs_node_t *tmpfs_readdir(vfs_node_t *node, uint32_t index)
{
    if (!node || !(node->flags & VFS_DIRECTORY))
        return NULL;

    tmpfs_data_t *data = (tmpfs_data_t *)node->filesystem;
    if (!data || index >= data->child_count)
        return NULL;

    return data->children[index];
}

// tmpfs finddir function
static vfs_node_t *tmpfs_finddir(vfs_node_t *node, const char *name)
{
    if (!node || !(node->flags & VFS_DIRECTORY))
        return NULL;

    tmpfs_data_t *data = (tmpfs_data_t *)node->filesystem;
    if (!data)
        return NULL;

    for (uint32_t i = 0; i < data->child_count; i++)
    {
        vfs_node_t *child = data->children[i];

        // Simple string comparison
        const char *n1 = child->name;
        const char *n2 = name;
        int match = 1;
        while (*n1 && *n2)
        {
            if (*n1++ != *n2++)
            {
                match = 0;
                break;
            }
        }
        if (match && *n1 == *n2)
        {
            return child;
        }
    }

    return NULL;
}

// Create a new tmpfs node
vfs_node_t *tmpfs_create_file(vfs_node_t *parent, const char *name, uint32_t flags)
{
    if (!parent || !(parent->flags & VFS_DIRECTORY))
        return NULL;

    tmpfs_data_t *parent_data = (tmpfs_data_t *)parent->filesystem;
    if (!parent_data || parent_data->child_count >= MAX_FILES)
        return NULL;

    // Allocate new node
    static vfs_node_t nodes[MAX_FILES];
    static int next_node = 0;

    if (next_node >= MAX_FILES)
        return NULL;

    vfs_node_t *node = &nodes[next_node++];

    // Copy name
    int i = 0;
    while (name[i] && i < 255)
    {
        node->name[i] = name[i];
        i++;
    }
    node->name[i] = '\0';

    node->flags = flags;
    node->inode = next_inode++;
    node->length = 0;
    node->parent = parent;

    // Allocate data
    node->filesystem = tmpfs_alloc_data();

    // Set function pointers
    node->read = tmpfs_read;
    node->write = tmpfs_write;
    node->open = NULL;
    node->close = NULL;
    node->readdir = tmpfs_readdir;
    node->finddir = tmpfs_finddir;

    // Add to parent
    parent_data->children[parent_data->child_count++] = node;

    return node;
}

void tmpfs_init(void)
{
    serial_print("Initializing tmpfs...\n");

    // Create root directory
    static vfs_node_t root;
    root.name[0] = '/';
    root.name[1] = '\0';
    root.flags = VFS_DIRECTORY;
    root.inode = 0;
    root.length = 0;
    root.parent = NULL;
    root.filesystem = tmpfs_alloc_data();

    root.read = tmpfs_read;
    root.write = tmpfs_write;
    root.readdir = tmpfs_readdir;
    root.finddir = tmpfs_finddir;
    root.open = NULL;
    root.close = NULL;

    // Set as VFS root
    extern void vfs_set_root(vfs_node_t * root);
    vfs_set_root(&root);

    serial_print("tmpfs mounted as root\n");

    // Create some test files
    vfs_node_t *readme = tmpfs_create_file(&root, "readme.txt", VFS_FILE);
    if (readme)
    {
        const char *content = "Welcome to MyOS!\nThis is a test file.\n";
        int len = 0;
        while (content[len])
            len++;
        tmpfs_write(readme, 0, len, (uint8_t *)content);
        serial_print("Created /readme.txt\n");
    }

    // Create a directory
    vfs_node_t *docs = tmpfs_create_file(&root, "docs", VFS_DIRECTORY);
    if (docs)
    {
        serial_print("Created /docs directory\n");

        // Create file in docs
        vfs_node_t *test = tmpfs_create_file(docs, "test.txt", VFS_FILE);
        if (test)
        {
            const char *content = "Test file in docs directory\n";
            int len = 0;
            while (content[len])
                len++;
            tmpfs_write(test, 0, len, (uint8_t *)content);
            serial_print("Created /docs/test.txt\n");
        }
    }
}