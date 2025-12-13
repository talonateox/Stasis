#include "page_frame_alloc.h"

#include "../../io/terminal.h"
#include "../memmap.h"

uint64_t free_memory;
uint64_t used_memory;
bool initialized = false;

pfallocator_t _g_alloc = {0};

void pfallocator_init(size_t offset) {
    if (initialized)
        return;

    _g_alloc.offset = offset;
    initialized = true;

    void *largest_free_segment = NULL;
    size_t largest_free_segment_size = 0;
    uint64_t highest_address = 0;

    for (size_t i = 0; i < memmap_get_entry_count(); i++) {
        struct limine_memmap_entry *entry = memmap_get_entry(i);

        uint64_t region_end = entry->base + entry->length;
        if (region_end > highest_address) {
            highest_address = region_end;
        }

        if (entry->type == LIMINE_MEMMAP_USABLE) {
            if (entry->length > largest_free_segment_size) {
                largest_free_segment = (void *)(entry->base + offset);
                largest_free_segment_size = entry->length;
            }
        }
    }

    _g_alloc.page_count = highest_address / PAGE_SIZE;

    uint64_t refcount_array_size = _g_alloc.page_count * sizeof(uint16_t);
    uint64_t refcount_pages = (refcount_array_size + PAGE_SIZE - 1) / PAGE_SIZE;

    _g_alloc.refcounts = (uint16_t *)largest_free_segment;
    _g_alloc.page_index = 0;

    for (size_t i = 0; i < _g_alloc.page_count; i++) {
        _g_alloc.refcounts[i] = 1;
    }

    free_memory = 0;
    used_memory = 0;

    for (size_t i = 0; i < memmap_get_entry_count(); i++) {
        struct limine_memmap_entry *entry = memmap_get_entry(i);
        if (entry->type == LIMINE_MEMMAP_USABLE) {
            void *virt_addr = (void *)(entry->base + offset);
            uint64_t page_count = entry->length / PAGE_SIZE;

            for (uint64_t j = 0; j < page_count; j++) {
                void *page = (void *)((uint64_t)virt_addr + j * PAGE_SIZE);
                uint64_t index = ((uint64_t)page - offset) / PAGE_SIZE;
                if (index < _g_alloc.page_count) {
                    _g_alloc.refcounts[index] = 0;
                    free_memory += PAGE_SIZE;
                }
            }
        }
    }

    pfallocator_lock_pages(largest_free_segment, refcount_pages);
}

void *pfallocator_request_page() {
    for (uint64_t i = _g_alloc.page_index; i < _g_alloc.page_count; i++) {
        if (_g_alloc.refcounts[i] != 0)
            continue;

        void *addr = (void *)(i * PAGE_SIZE + _g_alloc.offset);
        _g_alloc.refcounts[i] = 1;
        free_memory -= PAGE_SIZE;
        used_memory += PAGE_SIZE;
        _g_alloc.page_index = i + 1;
        return addr;
    }

    for (uint64_t i = 0; i < _g_alloc.page_index; i++) {
        if (_g_alloc.refcounts[i] != 0)
            continue;

        void *addr = (void *)(i * PAGE_SIZE + _g_alloc.offset);
        _g_alloc.refcounts[i] = 1;
        free_memory -= PAGE_SIZE;
        used_memory += PAGE_SIZE;
        _g_alloc.page_index = i + 1;
        return addr;
    }

    return NULL;
}

void pfallocator_ref_page(void *address) {
    if ((uint64_t)address < _g_alloc.offset)
        return;

    uint64_t i = ((uint64_t)address - _g_alloc.offset) / PAGE_SIZE;
    if (i >= _g_alloc.page_count) {
        printkf_error("ref_page(): index %llu >= page_count %llu (addr %p)\n", i, _g_alloc.page_count, address);
        return;
    }

    if (_g_alloc.refcounts[i] == 0) {
        printkf_error("ref_page(): page %p (index %llu) has refcount 0!\n", address, i);
        return;
    }
    if (_g_alloc.refcounts[i] == UINT16_MAX)
        return;

    _g_alloc.refcounts[i]++;
}

uint16_t pfallocator_unref_page(void *address) {
    if ((uint64_t)address < _g_alloc.offset)
        return UINT16_MAX;

    uint64_t i = ((uint64_t)address - _g_alloc.offset) / PAGE_SIZE;
    if (i >= _g_alloc.page_count) {
        printkf_error("unref_page(): index %llu >= page_count %llu (addr %p)\n", i, _g_alloc.page_count, address);
        return UINT16_MAX;
    }

    if (_g_alloc.refcounts[i] == 0)
        return 0;

    _g_alloc.refcounts[i]--;

    if (_g_alloc.refcounts[i] == 0) {
        free_memory += PAGE_SIZE;
        used_memory -= PAGE_SIZE;
        if (_g_alloc.page_index > i)
            _g_alloc.page_index = i;
    }

    return _g_alloc.refcounts[i];
}

uint16_t pfallocator_get_refcount(void *address) {
    if ((uint64_t)address < _g_alloc.offset)
        return UINT16_MAX;

    uint64_t i = ((uint64_t)address - _g_alloc.offset) / PAGE_SIZE;
    if (i >= _g_alloc.page_count)
        return UINT16_MAX;

    return _g_alloc.refcounts[i];
}

void pfallocator_free_page(void *address) {
    pfallocator_unref_page(address);
}

void pfallocator_free_pages(void *address, uint64_t count) {
    for (size_t i = 0; i < count; i++) {
        pfallocator_unref_page((void *)((uint64_t)address + (i * PAGE_SIZE)));
    }
}

void pfallocator_lock_page(void *address) {
    if ((uint64_t)address < _g_alloc.offset)
        return;

    uint64_t i = ((uint64_t)address - _g_alloc.offset) / PAGE_SIZE;
    if (i >= _g_alloc.page_count)
        return;

    if (_g_alloc.refcounts[i] == 0) {
        _g_alloc.refcounts[i] = 1;
        free_memory -= PAGE_SIZE;
        used_memory += PAGE_SIZE;
    }
}

void pfallocator_lock_pages(void *address, uint64_t count) {
    for (size_t i = 0; i < count; i++) {
        pfallocator_lock_page((void *)((uint64_t)address + (i * PAGE_SIZE)));
    }
}

uint64_t pfallocator_get_free_ram() {
    return free_memory;
}

uint64_t pfallocator_get_used_ram() {
    return used_memory;
}
