#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define PAGE_SIZE 4096

typedef struct {
    uint16_t* refcounts;
    size_t page_count;
    size_t offset;
    size_t page_index;
} pfallocator_t;

void pfallocator_init(size_t offset);

uint64_t pfallocator_get_free_ram();
uint64_t pfallocator_get_used_ram();

void* pfallocator_request_page();
void pfallocator_ref_page(void* address);
uint16_t pfallocator_unref_page(void* address);
uint16_t pfallocator_get_refcount(void* address);

void pfallocator_free_page(void* address);      // Same as unref_page
void pfallocator_free_pages(void* address, uint64_t count);
void pfallocator_lock_page(void* address);      // Sets refcount to 1 if 0
void pfallocator_lock_pages(void* address, uint64_t count);
