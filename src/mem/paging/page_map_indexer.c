#include "page_map_indexer.h"

page_map_indexer_t page_map_indexer_new(uint64_t virtual_address) {
    page_map_indexer_t indexer = {0};

    virtual_address >>= 12;
    indexer.p = virtual_address & 0x1ff;
    virtual_address >>= 9;
    indexer.pt = virtual_address & 0x1ff;
    virtual_address >>= 9;
    indexer.pd = virtual_address & 0x1ff;
    virtual_address >>= 9;
    indexer.pdp = virtual_address & 0x1ff;

    return indexer;
}