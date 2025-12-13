#pragma once

#include <stdint.h>

typedef struct {
    uint64_t pdp;
    uint64_t pd;
    uint64_t pt;
    uint64_t p;
} page_map_indexer_t;

page_map_indexer_t page_map_indexer_new(uint64_t virtual_address);