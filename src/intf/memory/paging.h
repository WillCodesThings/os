#pragma once
#include <stdint.h>

void paging_init(void);
uint64_t paging_get_total_memory(void);
uint32_t paging_get_table_count(void);
