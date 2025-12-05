#include "paging.h"

#include "page_table_manager.h"
#include "../memmap.h"
#include "../alloc/page_frame_alloc.h"
#include "../../limine.h"
#include "../../std/string.h"
#include "../../io/terminal.h"

page_table_manager_t _g_page_table_manager = {};

void map_memory_regions(uint64_t offset) {
    printkf_info("Mapping memory regions...\n");

    uint64_t total_pages_mapped = 0;

    for(size_t region = 0; region < memmap_get_entry_count(); region++) {
        struct limine_memmap_entry* entry = memmap_get_entry(region);

        uint64_t base = entry->base & ~0xFFF;
        uint64_t top = (entry->base + entry->length + 0xFFF) & ~0xFFF;
        uint64_t pages = (top - base) / PAGE_SIZE;

        for(uint64_t i = 0; i < pages; i++) {
            uint64_t addr = base + (i * PAGE_SIZE);

            if(!page_table_map(&_g_page_table_manager, (void*)addr, (void*)addr)) {
                panic("[MMU] Failed to map %k%p%r\n", 0xcccc66, (void*)addr);
            }

            if(!page_table_map(&_g_page_table_manager, (void*)(addr + offset), (void*)addr)) {
                panic("[MMU] Failed to map HHDM %k%p%r\n", 0xcccc66, (void*)(addr + offset));
            }

            total_pages_mapped += 2;
        }
    }

    printkf_ok("Mapped %k%llu%r pages\n", 0xcccc66, total_pages_mapped);
}

void map_kernel(uint64_t kernel_start, uint64_t kernel_end) {
    printkf_info("Mapping kernel...\n");
    uint64_t kernel_virt_start = kernel_start;
    uint64_t kernel_virt_end = kernel_end;
    uint64_t kernel_phys_base = 0;

    for(size_t i = 0; i < memmap_get_entry_count(); i++) {
        struct limine_memmap_entry* entry = memmap_get_entry(i);
        if(entry->type == LIMINE_MEMMAP_EXECUTABLE_AND_MODULES) {
            kernel_phys_base = entry->base;
            break;
        }
    }

    uint64_t kernel_pages = ((kernel_virt_end - kernel_virt_start) + PAGE_SIZE - 1) / PAGE_SIZE;
    for(uint64_t i = 0; i < kernel_pages; i++) {
        page_table_map(&_g_page_table_manager,
                      (void*)(kernel_virt_start + i * PAGE_SIZE),
                      (void*)(kernel_phys_base + i * PAGE_SIZE));
    }
    printkf_ok("Kernel mapped %k%llu%r pages\n", 0xcccc66, kernel_pages);
}

void page_table_init(uint64_t offset, uint64_t kernel_start, uint64_t kernel_end) {
    page_table_t* pml4 = (page_table_t*)pfallocator_request_page();
    memset(pml4, 0, 0x1000);

    _g_page_table_manager = (page_table_manager_t){pml4, offset};

    map_memory_regions(offset);
    map_kernel(kernel_start, kernel_end);

    uint64_t pml4_phys = (uint64_t)pml4 - offset;
    asm volatile("mov %0, %%cr3" : : "r" (pml4_phys) : "memory");
}

void page_map_memory(void* virt, void* phys) {
    page_table_map(&_g_page_table_manager,  virt, phys);
}

void page_direntry_set_flag(page_direntry_t* entry, page_direntry_flag_t flag, bool enabled) {
    uint64_t bit = (uint64_t)1 << flag;
    entry->value &= ~bit;
    if(enabled) entry->value |= bit;
}

bool page_direntry_get_flag(page_direntry_t* entry, page_direntry_flag_t flag) {
    uint64_t bit = (uint64_t)1 << flag;
    return (entry->value & bit) > 0;
}

void page_direntry_set_address(page_direntry_t* entry, uint64_t address) {
    address &= 0x000000ffffffffff;
    entry->value &= 0xfff0000000000fff;
    entry->value |= (address << 12);
}

uint64_t page_direntry_get_address(page_direntry_t* entry) {
    return (entry->value & 0x000ffffffffff000) >> 12;
}
