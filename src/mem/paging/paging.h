#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    PAGE_PRESENT = 0,
    PAGE_READ_WRITE = 1,
    PAGE_USER_SUPER = 2,
    PAGE_WRITE_THROUGH = 3,
    PAGE_CACHE_DISABLED = 4,
    PAGE_ACCESSED = 5,
    PAGE_LARGER_PAGES = 6,
    PAGE_COW = 9,
    PAGE_NX = 63,
} page_direntry_flag_t;

typedef struct {
    uint64_t value;
} page_direntry_t;

typedef struct {
    page_direntry_t entries[512];
} __attribute__((aligned(0x1000))) page_table_t;

void page_direntry_set_flag(page_direntry_t* entry, page_direntry_flag_t flag,
                            bool enabled);
bool page_direntry_get_flag(page_direntry_t* entry, page_direntry_flag_t flag);
void page_direntry_set_address(page_direntry_t* entry, uint64_t address);
uint64_t page_direntry_get_address(page_direntry_t* entry);

void page_table_init(uint64_t offset, uint64_t kernel_start,
                     uint64_t kernel_end);
page_table_t* page_table_create_user();
page_table_t* page_table_clone_for_user();
void page_table_destroy_user(page_table_t* pml4);

void page_map_memory(void* virt, void* phys);
void page_map_memory_to(page_table_t* pml4, void* virt, void* phys);

size_t page_get_offset();
page_table_t* page_get_pml4();
void* page_table_get_physical_from(page_table_t* pml4, void* virt);
page_direntry_t* page_table_get_pte(page_table_t* pml4, void* virt);
bool page_handle_cow_fault(void* fault_addr);
