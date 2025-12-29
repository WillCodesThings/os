#include <fs/vfs.h>
#include <shell/print.h>

#include <memory/heap.h>

static vfs_node_t *vfs_root = NULL;

// String helper functions
static int strncmp(const char *s1, const char *s2, size_t n)
{
    while (n && *s1 && (*s1 == *s2))
    {
        s1++;
        s2++;
        n--;
    }
    if (n == 0)
        return 0;
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

static size_t strlen(const char *str)
{
    size_t len = 0;
    while (str[len])
        len++;
    return len;
}

static void strcpy(char *dest, const char *src)
{
    while (*src)
    {
        *dest++ = *src++;
    }
    *dest = '\0';
}

static char *strdup(const char *str)
{
    size_t len = strlen(str) + 1;
    char *copy = (char *)kmalloc(len);
    if (copy)
    {
        for (size_t i = 0; i < len; i++)
        {
            copy[i] = str[i];
        }
    }
    return copy;
}

void vfs_init(void)
{
    serial_print("Initializing VFS...\n");
    vfs_root = NULL;
}

vfs_node_t *vfs_get_root(void)
{
    return vfs_root;
}

// Set the VFS root (called by filesystem mount)
void vfs_set_root(vfs_node_t *root)
{
    vfs_root = root;
}

// Resolve a path to a VFS node
vfs_node_t *vfs_resolve_path(const char *path)
{
    if (!vfs_root)
        return NULL;
    if (!path || path[0] != '/')
        return NULL;

    // Root directory
    if (path[1] == '\0')
    {
        return vfs_root;
    }

    vfs_node_t *current = vfs_root;
    const char *p = path + 1; // Skip leading '/'
    char component[256];
    int i;

    while (*p)
    {
        // Extract next path component
        i = 0;
        while (*p && *p != '/' && i < 255)
        {
            component[i++] = *p++;
        }
        component[i] = '\0';

        if (*p == '/')
            p++; // Skip '/'

        // Look up component in current directory
        if (current->finddir)
        {
            current = current->finddir(current, component);
            if (!current)
                return NULL;
        }
        else
        {
            return NULL;
        }
    }

    return current;
}

vfs_node_t *vfs_open(const char *path, uint32_t flags)
{
    vfs_node_t *node = vfs_resolve_path(path);
    if (!node)
        return NULL;

    if (node->open)
    {
        node->open(node, flags);
    }

    return node;
}

void vfs_close(vfs_node_t *node)
{
    if (node && node->close)
    {
        node->close(node);
    }
}

int vfs_read(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer)
{
    if (!node || !node->read)
        return -1;
    return node->read(node, offset, size, buffer);
}

int vfs_write(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer)
{
    if (!node || !node->write)
        return -1;
    return node->write(node, offset, size, buffer);
}

vfs_node_t *vfs_readdir(vfs_node_t *node, uint32_t index)
{
    if (!node || !node->readdir)
        return NULL;
    return node->readdir(node, index);
}

vfs_node_t *vfs_finddir(vfs_node_t *node, const char *name)
{
    if (!node || !node->finddir)
        return NULL;
    return node->finddir(node, name);
}

const char *vfs_get_filename(const char *path)
{
    const char *last_slash = path;
    while (*path)
    {
        if (*path == '/')
            last_slash = path + 1;
        path++;
    }
    return last_slash;
}

int vfs_mount(const char *path, vfs_node_t *mountpoint)
{
    if (!path || !mountpoint)
        return -1;
    vfs_node_t *node = vfs_resolve_path(path);
    if (!node)
        return -1;

    node->mountpoint = mountpoint;
    vfs_root->parent = node;
    return 0;
}