#include <memory/paging.h>
#include <memory/heap.h>
#include <utils/log.h>
#include <stdint.h>

// Provided by boot assembly after parsing the Multiboot2 memory map
extern uint64_t total_physical_memory;

// Page table entry flags
#define PTE_PRESENT  (1ULL << 0)
#define PTE_WRITABLE (1ULL << 1)
#define PTE_HUGE     (1ULL << 7) // 2MB page (used in PD entries)

// Each PD table covers 1GB (512 entries * 2MB huge pages)
#define GB                (1ULL << 30)
#define MB                (1ULL << 20)
#define PAGE_TABLE_SIZE   4096
#define ENTRIES_PER_TABLE 512
#define MIN_MAPPED_BYTES  (4ULL * GB) // Always map at least 4GB for MMIO/framebuffer

static uint32_t num_pd_tables = 0;
static uint32_t num_pdpt_tables = 0;
static uint32_t total_tables = 0;

void paging_init(void) {
    uint64_t total_mem = total_physical_memory;

    LOG_INFO("MEM", "Detected %u MB physical memory", (uint32_t)(total_mem / MB));

    if (total_mem == 0) {
        LOG_WARN("MEM", "No memory map detected, skipping paging init");
        return;
    }

    // Map at least 4GB to cover MMIO regions (framebuffer, PCI devices),
    // or more if physical RAM exceeds 4GB
    uint64_t map_size = total_mem > MIN_MAPPED_BYTES ? total_mem : MIN_MAPPED_BYTES;

    // Calculate number of PD tables needed (each covers 1GB with 2MB huge pages)
    num_pd_tables = (uint32_t)((map_size + GB - 1) / GB);
    if (num_pd_tables == 0)
        num_pd_tables = 1;

    // Calculate number of PDPT tables (each holds 512 PD pointers)
    num_pdpt_tables = (num_pd_tables + ENTRIES_PER_TABLE - 1) / ENTRIES_PER_TABLE;
    if (num_pdpt_tables == 0)
        num_pdpt_tables = 1;

    // Total: 1 PML4 + PDPTs + PDs
    total_tables = 1 + num_pdpt_tables + num_pd_tables;

    LOG_INFO("MEM", "Allocating %u page tables (1 PML4 + %u PDPT + %u PD)", total_tables,
             num_pdpt_tables, num_pd_tables);

    // Allocate PML4 table
    uint64_t *pml4 = (uint64_t *)kmalloc_aligned(PAGE_TABLE_SIZE, PAGE_TABLE_SIZE);
    if (!pml4) {
        LOG_ERROR("MEM", "Failed to allocate PML4");
        return;
    }
    for (int i = 0; i < ENTRIES_PER_TABLE; i++) pml4[i] = 0;

    // Allocate PDPT table(s)
    uint64_t *pdpt_tables[1] = {0}; // Realistically only 1 PDPT for < 512GB
    for (uint32_t i = 0; i < num_pdpt_tables; i++) {
        pdpt_tables[i] = (uint64_t *)kmalloc_aligned(PAGE_TABLE_SIZE, PAGE_TABLE_SIZE);
        if (!pdpt_tables[i]) {
            LOG_ERROR("MEM", "Failed to allocate PDPT");
            return;
        }
        for (int j = 0; j < ENTRIES_PER_TABLE; j++) pdpt_tables[i][j] = 0;

        // Point PML4 entry to this PDPT
        pml4[i] = (uint64_t)(uintptr_t)pdpt_tables[i] | PTE_PRESENT | PTE_WRITABLE;
    }

    // Allocate PD tables and fill with 2MB huge page identity mapping
    for (uint32_t i = 0; i < num_pd_tables; i++) {
        uint64_t *pd = (uint64_t *)kmalloc_aligned(PAGE_TABLE_SIZE, PAGE_TABLE_SIZE);
        if (!pd) {
            LOG_ERROR("MEM", "Failed to allocate PD");
            return;
        }

        // Fill PD entries: each maps a 2MB region
        for (int j = 0; j < ENTRIES_PER_TABLE; j++) {
            uint64_t phys_addr = ((uint64_t)i * GB) + ((uint64_t)j * (2 * MB));
            pd[j] = phys_addr | PTE_PRESENT | PTE_WRITABLE | PTE_HUGE;
        }

        // Point the correct PDPT entry to this PD
        uint32_t pdpt_idx = i / ENTRIES_PER_TABLE;
        uint32_t entry_idx = i % ENTRIES_PER_TABLE;
        pdpt_tables[pdpt_idx][entry_idx] = (uint64_t)(uintptr_t)pd | PTE_PRESENT | PTE_WRITABLE;
    }

    // Load new PML4 into CR3
    uint64_t pml4_addr = (uint64_t)(uintptr_t)pml4;
    __asm__ volatile("mov %0, %%cr3" : : "r"(pml4_addr) : "memory");

    LOG_INFO("MEM", "Page tables allocated and loaded into CR3");
}

uint64_t paging_get_total_memory(void) {
    return total_physical_memory;
}

uint32_t paging_get_table_count(void) {
    return total_tables;
}
