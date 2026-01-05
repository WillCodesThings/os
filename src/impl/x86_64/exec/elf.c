#include "exec/elf.h"
#include "memory/heap.h"
#include "utils/memory.h"

// Validate ELF file
int elf_validate(const uint8_t *data, size_t size) {
    if (size < sizeof(elf64_header_t)) {
        return -1;  // Too small
    }

    elf64_header_t *header = (elf64_header_t *)data;

    // Check magic number
    if (*(uint32_t *)header->e_ident != ELF_MAGIC) {
        return -2;  // Invalid magic
    }

    // Check class (must be 64-bit)
    if (header->e_ident[4] != ELFCLASS64) {
        return -3;  // Not 64-bit
    }

    // Check endianness (must be little-endian)
    if (header->e_ident[5] != ELFDATA2LSB) {
        return -4;  // Not little-endian
    }

    // Check machine type
    if (header->e_machine != EM_X86_64) {
        return -5;  // Not x86_64
    }

    // Check type (must be executable)
    if (header->e_type != ET_EXEC) {
        return -6;  // Not executable
    }

    return 0;  // Valid
}

// Get entry point from ELF
uint64_t elf_get_entry(const uint8_t *data) {
    elf64_header_t *header = (elf64_header_t *)data;
    return header->e_entry;
}

// Load ELF into memory
int elf_load(const uint8_t *data, size_t size, elf_info_t *info) {
    int ret = elf_validate(data, size);
    if (ret != 0) {
        return ret;
    }

    elf64_header_t *header = (elf64_header_t *)data;

    // Find the memory bounds needed
    uint64_t min_addr = 0xFFFFFFFFFFFFFFFF;
    uint64_t max_addr = 0;

    for (uint16_t i = 0; i < header->e_phnum; i++) {
        elf64_phdr_t *phdr = (elf64_phdr_t *)(data + header->e_phoff + i * header->e_phentsize);

        if (phdr->p_type == PT_LOAD) {
            if (phdr->p_vaddr < min_addr) {
                min_addr = phdr->p_vaddr;
            }
            if (phdr->p_vaddr + phdr->p_memsz > max_addr) {
                max_addr = phdr->p_vaddr + phdr->p_memsz;
            }
        }
    }

    if (min_addr >= max_addr) {
        return -7;  // No loadable segments
    }

    // Calculate total memory needed
    size_t total_size = max_addr - min_addr;

    // Allocate memory for the program (page-aligned)
    void *program_mem = kmalloc_aligned(total_size, 4096);
    if (!program_mem) {
        return -8;  // Out of memory
    }

    // Zero the memory
    memset(program_mem, 0, total_size);

    // Load each PT_LOAD segment
    for (uint16_t i = 0; i < header->e_phnum; i++) {
        elf64_phdr_t *phdr = (elf64_phdr_t *)(data + header->e_phoff + i * header->e_phentsize);

        if (phdr->p_type == PT_LOAD) {
            // Calculate destination in our allocated memory
            uint8_t *dest = (uint8_t *)program_mem + (phdr->p_vaddr - min_addr);

            // Copy the file portion
            if (phdr->p_filesz > 0) {
                memcpy(dest, data + phdr->p_offset, phdr->p_filesz);
            }

            // The rest (p_memsz - p_filesz) is BSS and already zeroed
        }
    }

    // Fill in the info structure
    info->entry_point = header->e_entry;
    info->base_addr = min_addr;
    info->end_addr = max_addr;
    info->program_memory = program_mem;
    info->memory_size = total_size;

    return 0;
}

// Unload ELF and free memory
void elf_unload(elf_info_t *info) {
    if (info && info->program_memory) {
        kfree(info->program_memory);
        info->program_memory = 0;
        info->memory_size = 0;
    }
}
