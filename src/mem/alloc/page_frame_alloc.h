#pragma once

#include <stddef.h>
#include "../../std/bitmap.h"
#include <stdint.h>
#include <stdbool.h>

#define PAGE_SIZE 4096

typedef struct {
    bitmap_t bitmap;
    size_t offset;
    size_t page_index;
} pfallocator_t;

void pfallocator_init(size_t offset);
void pfallocator_init_bitmap(size_t size, void* buffer);

uint64_t pfallocator_get_free_ram();
uint64_t pfallocator_get_used_ram();

void pfallocator_free_page(void* address);
void pfallocator_free_pages(void* address, uint64_t count);
void pfallocator_lock_page(void* address);
void pfallocator_lock_pages(void* address, uint64_t count);

void* pfallocator_request_page();

