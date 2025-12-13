#pragma once

#include "paging.h"

typedef struct {
    page_table_t *pml4;
    uint64_t offset;
} page_table_manager_t;

bool page_table_map(page_table_manager_t *manager, void *virt, void *phys);
