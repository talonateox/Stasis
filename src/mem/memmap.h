#pragma once

#include "../limine.h"
#include <stddef.h>

typedef struct {
    struct limine_memmap_entry **entries;
    size_t entry_count;
} memmap_t;

void memmap_init(struct limine_memmap_entry **entries, size_t entry_count);
size_t memmap_get_entry_count();
struct limine_memmap_entry *memmap_get_entry(int i);
size_t memmap_get_total();
void memmap_print();