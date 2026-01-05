// Add to a new file: src/utils/memory.c

#include <utils/memory.h>

// optimized memset
void *memset(void *ptr, int value, size_t num)
{
    unsigned char *p = (unsigned char *)ptr;
    unsigned char val = (unsigned char)value;
    
    // Fill 8 bytes at a time if possible
    if (num >= 8 && ((uintptr_t)ptr & 7) == 0)
    {
        uint64_t val64 = val;
        val64 |= val64 << 8;
        val64 |= val64 << 16;
        val64 |= val64 << 32;
        
        uint64_t *p64 = (uint64_t *)ptr;
        size_t chunks = num / 8;
        
        for (size_t i = 0; i < chunks; i++)
        {
            p64[i] = val64;
        }
        
        // Fill remaining bytes
        p += chunks * 8;
        num -= chunks * 8;
    }
    
    // Fill remaining bytes
    for (size_t i = 0; i < num; i++)
    {
        p[i] = val;
    }
    
    return ptr;
}

void *memcpy(void *dest, const void *src, size_t num)
{
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    
    for (size_t i = 0; i < num; i++)
    {
        d[i] = s[i];
    }
    
    return dest;
}

void *memmove(void *dest, const void *src, size_t num)
{
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    
    // Handle overlapping memory regions
    if (d < s)
    {
        // Copy forward
        for (size_t i = 0; i < num; i++)
        {
            d[i] = s[i];
        }
    }
    else if (d > s)
    {
        // Copy backward
        for (size_t i = num; i > 0; i--)
        {
            d[i - 1] = s[i - 1];
        }
    }
    
    return dest;
}

int memcmp(const void *ptr1, const void *ptr2, size_t num)
{
    const unsigned char *p1 = (const unsigned char *)ptr1;
    const unsigned char *p2 = (const unsigned char *)ptr2;
    
    for (size_t i = 0; i < num; i++)
    {
        if (p1[i] != p2[i])
        {
            return p1[i] - p2[i];
        }
    }
    
    return 0;
}