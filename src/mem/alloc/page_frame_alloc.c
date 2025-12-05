#include "page_frame_alloc.h"

#include "../memmap.h"
#include "../../io/terminal.h"

uint64_t free_memory;
uint64_t used_memory;
bool initialized = false;

pfallocator_t _g_alloc = {0};

void pfallocator_init(size_t offset) {
    if(initialized) return;

    _g_alloc.offset = offset;
    initialized = true;

    void* largest_free_segment = NULL;
    size_t largest_free_segment_size = 0;

    for(size_t i = 0; i < memmap_get_entry_count(); i++) {
        struct limine_memmap_entry* entry = memmap_get_entry(i);
        if(entry->type == LIMINE_MEMMAP_USABLE) {
            if(entry->length > largest_free_segment_size) {
                largest_free_segment = (void*)(entry->base + offset);
                largest_free_segment_size = entry->length;
            }
        }
    }

    uint64_t mem_size = memmap_get_total();
    uint64_t bitmap_size = mem_size / PAGE_SIZE / 8 + 1;

    pfallocator_init_bitmap(bitmap_size, largest_free_segment);

    free_memory = 0;
    used_memory = 0;

    for(size_t i = 0; i < _g_alloc.bitmap.size * 8; i++) {
        bitmap_set(&_g_alloc.bitmap, i, true);
    }

    for(size_t i = 0; i < memmap_get_entry_count(); i++) {
        struct limine_memmap_entry* entry = memmap_get_entry(i);
        if(entry->type == LIMINE_MEMMAP_USABLE) {
            void* virt_addr = (void*)(entry->base + offset);
            uint64_t page_count = entry->length / PAGE_SIZE;

            for(uint64_t j = 0; j < page_count; j++) {
                void* page = (void*)((uint64_t)virt_addr + j * PAGE_SIZE);
                uint64_t index = ((uint64_t)page - offset) / PAGE_SIZE;
                if(index < _g_alloc.bitmap.size * 8) {
                    bitmap_set(&_g_alloc.bitmap, index, false);
                    free_memory += PAGE_SIZE;
                }
            }
        }
    }

    uint64_t bitmap_pages = (bitmap_size + PAGE_SIZE - 1) / PAGE_SIZE;
    pfallocator_lock_pages(largest_free_segment, bitmap_pages);
}

void pfallocator_init_bitmap(size_t size, void* buffer) {
    _g_alloc.bitmap.size = size;
    _g_alloc.bitmap.buffer = (uint8_t*)buffer;

    for(size_t i = 0; i < size; i++) {
        *(uint8_t*)(_g_alloc.bitmap.buffer + i) = 0;
    }
}

void* pfallocator_request_page() {
    for(; _g_alloc.page_index < _g_alloc.bitmap.size * 8; _g_alloc.page_index++) {
        uint64_t i = _g_alloc.page_index;
        if(bitmap_get(&_g_alloc.bitmap, i) == true) continue;

        void* addr = (void*)(i * PAGE_SIZE + _g_alloc.offset);
        pfallocator_lock_page(addr);
        return addr;
    }

    return NULL;
}

void pfallocator_free_page(void* address) {
    if((uint64_t)address < _g_alloc.offset) return;

    uint64_t i = ((uint64_t)address - _g_alloc.offset) / PAGE_SIZE;

    if(i >= _g_alloc.bitmap.size * 8) return;

    if(bitmap_get(&_g_alloc.bitmap, i) == false) return;

    bitmap_set(&_g_alloc.bitmap, i, false);
    free_memory += 4096;
    used_memory -= 4096;

    if(_g_alloc.page_index > i) _g_alloc.page_index = i;
}

void pfallocator_free_pages(void* address, uint64_t count) {
    for(size_t i = 0; i < count; i++) {
        pfallocator_free_page((void*)((uint64_t)address + (i * PAGE_SIZE)));
    }
}

void pfallocator_lock_page(void* address) {
    if((uint64_t)address < _g_alloc.offset) return;

    uint64_t i = ((uint64_t)address - _g_alloc.offset) / PAGE_SIZE;

    if(i >= _g_alloc.bitmap.size * 8) return;

    if(bitmap_get(&_g_alloc.bitmap, i) == true) return;

    bitmap_set(&_g_alloc.bitmap, i, true);
    free_memory -= 4096;
    used_memory += 4096;
}

void pfallocator_lock_pages(void* address, uint64_t count) {
    for(size_t i = 0; i < count; i++) {
        pfallocator_lock_page((void*)((uint64_t)address + (i * PAGE_SIZE)));
    }
}


uint64_t pfallocator_get_free_ram() {
    return free_memory;
}

uint64_t pfallocator_get_used_ram() {
    return used_memory;
}
