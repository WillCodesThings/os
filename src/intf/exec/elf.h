#pragma once
#include <stdint.h>
#include <stddef.h>

// ELF Magic
#define ELF_MAGIC 0x464C457F  // "\x7FELF" in little-endian

// ELF Class
#define ELFCLASS32 1
#define ELFCLASS64 2

// ELF Data encoding
#define ELFDATA2LSB 1  // Little endian
#define ELFDATA2MSB 2  // Big endian

// ELF Type
#define ET_NONE   0  // No file type
#define ET_REL    1  // Relocatable
#define ET_EXEC   2  // Executable
#define ET_DYN    3  // Shared object
#define ET_CORE   4  // Core file

// ELF Machine
#define EM_X86_64 62  // AMD x86-64

// Program header types
#define PT_NULL    0  // Unused
#define PT_LOAD    1  // Loadable segment
#define PT_DYNAMIC 2  // Dynamic linking info
#define PT_INTERP  3  // Interpreter path
#define PT_NOTE    4  // Auxiliary info
#define PT_SHLIB   5  // Reserved
#define PT_PHDR    6  // Program header table

// Program header flags
#define PF_X 0x1  // Execute
#define PF_W 0x2  // Write
#define PF_R 0x4  // Read

// ELF64 Header
typedef struct {
    uint8_t  e_ident[16];    // Magic number and other info
    uint16_t e_type;         // Object file type
    uint16_t e_machine;      // Architecture
    uint32_t e_version;      // Object file version
    uint64_t e_entry;        // Entry point virtual address
    uint64_t e_phoff;        // Program header table file offset
    uint64_t e_shoff;        // Section header table file offset
    uint32_t e_flags;        // Processor-specific flags
    uint16_t e_ehsize;       // ELF header size
    uint16_t e_phentsize;    // Program header table entry size
    uint16_t e_phnum;        // Program header table entry count
    uint16_t e_shentsize;    // Section header table entry size
    uint16_t e_shnum;        // Section header table entry count
    uint16_t e_shstrndx;     // Section header string table index
} __attribute__((packed)) elf64_header_t;

// ELF64 Program Header
typedef struct {
    uint32_t p_type;    // Segment type
    uint32_t p_flags;   // Segment flags
    uint64_t p_offset;  // Segment file offset
    uint64_t p_vaddr;   // Segment virtual address
    uint64_t p_paddr;   // Segment physical address
    uint64_t p_filesz;  // Segment size in file
    uint64_t p_memsz;   // Segment size in memory
    uint64_t p_align;   // Segment alignment
} __attribute__((packed)) elf64_phdr_t;

// ELF64 Section Header
typedef struct {
    uint32_t sh_name;       // Section name (string table index)
    uint32_t sh_type;       // Section type
    uint64_t sh_flags;      // Section flags
    uint64_t sh_addr;       // Section virtual address
    uint64_t sh_offset;     // Section file offset
    uint64_t sh_size;       // Section size
    uint32_t sh_link;       // Link to another section
    uint32_t sh_info;       // Additional section info
    uint64_t sh_addralign;  // Section alignment
    uint64_t sh_entsize;    // Entry size if section holds table
} __attribute__((packed)) elf64_shdr_t;

// Loaded ELF info
typedef struct {
    uint64_t entry_point;   // Entry point address
    uint64_t base_addr;     // Base load address
    uint64_t end_addr;      // End of loaded segments
    void *program_memory;   // Allocated program memory
    size_t memory_size;     // Total allocated size
} elf_info_t;

// ELF functions
int elf_validate(const uint8_t *data, size_t size);
int elf_load(const uint8_t *data, size_t size, elf_info_t *info);
void elf_unload(elf_info_t *info);
uint64_t elf_get_entry(const uint8_t *data);
