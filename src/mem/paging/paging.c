#include "paging.h"

#include "../../io/terminal.h"
#include "../../limine.h"
#include "../../std/string.h"
#include "../alloc/page_frame_alloc.h"
#include "../memmap.h"
#include "page_map_indexer.h"
#include "page_table_manager.h"

page_table_manager_t _g_page_table_manager = {};

void map_memory_regions(uint64_t offset) {
    printkf_info("Mapping memory regions...\n");

    uint64_t total_pages_mapped = 0;

    for (size_t region = 0; region < memmap_get_entry_count(); region++) {
        struct limine_memmap_entry *entry = memmap_get_entry(region);

        uint64_t base = entry->base & ~0xFFF;
        uint64_t top = (entry->base + entry->length + 0xFFF) & ~0xFFF;
        uint64_t pages = (top - base) / PAGE_SIZE;

        for (uint64_t i = 0; i < pages; i++) {
            uint64_t addr = base + (i * PAGE_SIZE);

            if (!page_table_map(&_g_page_table_manager, (void *)addr, (void *)addr)) {
                panic("[MMU] Failed to map %k%p%r\n", 0xcccc66, (void *)addr);
            }

            if (!page_table_map(&_g_page_table_manager, (void *)(addr + offset), (void *)addr)) {
                panic("[MMU] Failed to map HHDM %k%p%r\n", 0xcccc66, (void *)(addr + offset));
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

    for (size_t i = 0; i < memmap_get_entry_count(); i++) {
        struct limine_memmap_entry *entry = memmap_get_entry(i);
        if (entry->type == LIMINE_MEMMAP_EXECUTABLE_AND_MODULES) {
            kernel_phys_base = entry->base;
            break;
        }
    }

    uint64_t kernel_pages = ((kernel_virt_end - kernel_virt_start) + PAGE_SIZE - 1) / PAGE_SIZE;
    for (uint64_t i = 0; i < kernel_pages; i++) {
        page_table_map(&_g_page_table_manager, (void *)(kernel_virt_start + i * PAGE_SIZE),
                       (void *)(kernel_phys_base + i * PAGE_SIZE));
    }
    printkf_ok("Kernel mapped %k%llu%r pages\n", 0xcccc66, kernel_pages);
}

void page_table_init(uint64_t offset, uint64_t kernel_start, uint64_t kernel_end) {
    page_table_t *pml4 = (page_table_t *)pfallocator_request_page();
    memset(pml4, 0, 0x1000);

    _g_page_table_manager = (page_table_manager_t){pml4, offset};

    map_memory_regions(offset);
    map_kernel(kernel_start, kernel_end);

    uint64_t pml4_phys = (uint64_t)pml4 - offset;
    __asm__ volatile("mov %0, %%cr3" : : "r"(pml4_phys) : "memory");
}

void page_map_memory(void *virt, void *phys) {
    page_table_map(&_g_page_table_manager, virt, phys);
}

void page_map_memory_to(page_table_t *pml4, void *virt, void *phys) {
    page_table_manager_t temp = {pml4, _g_page_table_manager.offset};
    page_table_map(&temp, virt, phys);
}

void page_direntry_set_flag(page_direntry_t *entry, page_direntry_flag_t flag, bool enabled) {
    uint64_t bit = (uint64_t)1 << flag;
    entry->value &= ~bit;
    if (enabled)
        entry->value |= bit;
}

bool page_direntry_get_flag(page_direntry_t *entry, page_direntry_flag_t flag) {
    uint64_t bit = (uint64_t)1 << flag;
    return (entry->value & bit) > 0;
}

void page_direntry_set_address(page_direntry_t *entry, uint64_t address) {
    address &= 0x000000ffffffffff;
    entry->value &= 0xfff0000000000fff;
    entry->value |= (address << 12);
}

uint64_t page_direntry_get_address(page_direntry_t *entry) {
    return (entry->value & 0x000ffffffffff000) >> 12;
}

size_t page_get_offset() {
    return _g_page_table_manager.offset;
}

static page_table_t *deep_copy_page_table(page_table_t *src, int level) {
    uint64_t offset = _g_page_table_manager.offset;

    page_table_t *dst = (page_table_t *)pfallocator_request_page();
    if (dst == NULL)
        return NULL;
    memset(dst, 0, 0x1000);

    for (int i = 0; i < 512; i++) {
        if (!page_direntry_get_flag(&src->entries[i], PAGE_PRESENT)) {
            continue;
        }

        if (level == 1) {
            if (page_direntry_get_flag(&src->entries[i], PAGE_READ_WRITE)) {
                page_direntry_set_flag(&src->entries[i], PAGE_READ_WRITE, false);
                page_direntry_set_flag(&src->entries[i], PAGE_COW, true);
            }
            dst->entries[i] = src->entries[i];

            uint64_t page_phys = page_direntry_get_address(&src->entries[i]) << 12;
            void *page = (void *)(page_phys + offset);
            pfallocator_ref_page(page);
        } else {
            uint64_t child_phys = page_direntry_get_address(&src->entries[i]) << 12;
            page_table_t *child_src = (page_table_t *)(child_phys + offset);

            page_table_t *child_dst = deep_copy_page_table(child_src, level - 1);
            if (child_dst == NULL) {
                return NULL;
            }

            uint64_t child_dst_phys = (uint64_t)child_dst - offset;
            dst->entries[i].value = src->entries[i].value;
            page_direntry_set_address(&dst->entries[i], child_dst_phys >> 12);
        }
    }

    return dst;
}

page_table_t *page_table_clone_for_user() {
    page_table_t *new_pml4 = (page_table_t *)pfallocator_request_page();
    if (new_pml4 == NULL)
        return NULL;

    memset(new_pml4, 0, 0x1000);

    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    page_table_t *current_pml4 = (page_table_t *)(cr3 + _g_page_table_manager.offset);
    uint64_t offset = _g_page_table_manager.offset;

    for (int i = 256; i < 512; i++) {
        new_pml4->entries[i] = current_pml4->entries[i];
    }

    for (int i = 0; i < 256; i++) {
        if (!page_direntry_get_flag(&current_pml4->entries[i], PAGE_PRESENT)) {
            continue;
        }
        uint64_t pdpt_phys = page_direntry_get_address(&current_pml4->entries[i]) << 12;
        page_table_t *pdpt_src = (page_table_t *)(pdpt_phys + offset);

        page_table_t *pdpt_dst = deep_copy_page_table(pdpt_src, 3);
        if (pdpt_dst == NULL) {
            return NULL;
        }

        uint64_t pdpt_dst_phys = (uint64_t)pdpt_dst - offset;
        new_pml4->entries[i].value = current_pml4->entries[i].value;
        page_direntry_set_address(&new_pml4->entries[i], pdpt_dst_phys >> 12);
    }

    __asm__ volatile("mov %0, %%cr3" : : "r"(cr3) : "memory");

    return new_pml4;
}

page_table_t *page_table_create_user() {
    page_table_t *new_pml4 = (page_table_t *)pfallocator_request_page();
    if (new_pml4 == NULL)
        return NULL;

    memset(new_pml4, 0, 0x1000);

    page_table_t *kernel_pml4 = _g_page_table_manager.pml4;

    for (int i = 256; i < 512; i++) {
        new_pml4->entries[i] = kernel_pml4->entries[i];
    }

    return new_pml4;
}

static void destroy_page_table_recursive(page_table_t *table, int level, bool free_leaf_pages) {
    uint64_t offset = _g_page_table_manager.offset;

    for (int i = 0; i < 512; i++) {
        if (!page_direntry_get_flag(&table->entries[i], PAGE_PRESENT)) {
            continue;
        }

        if (level > 1) {
            uint64_t child_phys = page_direntry_get_address(&table->entries[i]) << 12;
            page_table_t *child = (page_table_t *)(child_phys + offset);
            destroy_page_table_recursive(child, level - 1, free_leaf_pages);
        } else if (free_leaf_pages) {
            uint64_t page_phys = page_direntry_get_address(&table->entries[i]) << 12;
            void *page = (void *)(page_phys + offset);
            pfallocator_free_page(page);
        }
    }

    pfallocator_free_page(table);
}

static void page_table_destroy_internal(page_table_t *pml4, bool free_leaf_pages) {
    if (pml4 == NULL)
        return;
    if (pml4 == _g_page_table_manager.pml4)
        return;

    for (int i = 0; i < 256; i++) {
        if (!page_direntry_get_flag(&pml4->entries[i], PAGE_PRESENT)) {
            continue;
        }

        uint64_t pdpt_phys = page_direntry_get_address(&pml4->entries[i]) << 12;
        page_table_t *pdpt = (page_table_t *)(pdpt_phys + _g_page_table_manager.offset);
        destroy_page_table_recursive(pdpt, 3, free_leaf_pages);
    }

    pfallocator_free_page(pml4);
}

void page_table_destroy_user(page_table_t *pml4) {
    page_table_destroy_internal(pml4, true);
}

page_table_t *page_get_pml4() {
    return _g_page_table_manager.pml4;
}

void *page_table_get_physical(void *virt) {
    return page_table_get_physical_from(_g_page_table_manager.pml4, virt);
}

void *page_table_get_physical_from(page_table_t *pml4, void *virt) {
    page_map_indexer_t indexer = page_map_indexer_new((uint64_t)virt);
    uint64_t offset = _g_page_table_manager.offset;

    page_direntry_t pde = pml4->entries[indexer.pdp];
    if (!page_direntry_get_flag(&pde, PAGE_PRESENT))
        return NULL;

    uint64_t pdp_phys = page_direntry_get_address(&pde) << 12;
    page_table_t *pdp = (page_table_t *)(pdp_phys + offset);

    pde = pdp->entries[indexer.pd];
    if (!page_direntry_get_flag(&pde, PAGE_PRESENT))
        return NULL;

    uint64_t pd_phys = page_direntry_get_address(&pde) << 12;
    page_table_t *pd = (page_table_t *)(pd_phys + offset);

    pde = pd->entries[indexer.pt];
    if (!page_direntry_get_flag(&pde, PAGE_PRESENT))
        return NULL;

    uint64_t pt_phys = page_direntry_get_address(&pde) << 12;
    page_table_t *pt = (page_table_t *)(pt_phys + offset);

    pde = pt->entries[indexer.p];
    if (!page_direntry_get_flag(&pde, PAGE_PRESENT))
        return NULL;

    uint64_t page_phys = page_direntry_get_address(&pde) << 12;
    return (void *)page_phys;
}

page_direntry_t *page_table_get_pte(page_table_t *pml4, void *virt) {
    page_map_indexer_t indexer = page_map_indexer_new((uint64_t)virt);
    uint64_t offset = _g_page_table_manager.offset;

    page_direntry_t *pde = &pml4->entries[indexer.pdp];
    if (!page_direntry_get_flag(pde, PAGE_PRESENT))
        return NULL;

    uint64_t pdp_phys = page_direntry_get_address(pde) << 12;
    page_table_t *pdp = (page_table_t *)(pdp_phys + offset);

    pde = &pdp->entries[indexer.pd];
    if (!page_direntry_get_flag(pde, PAGE_PRESENT))
        return NULL;

    uint64_t pd_phys = page_direntry_get_address(pde) << 12;
    page_table_t *pd = (page_table_t *)(pd_phys + offset);

    pde = &pd->entries[indexer.pt];
    if (!page_direntry_get_flag(pde, PAGE_PRESENT))
        return NULL;

    uint64_t pt_phys = page_direntry_get_address(pde) << 12;
    page_table_t *pt = (page_table_t *)(pt_phys + offset);

    return &pt->entries[indexer.p];
}

bool page_handle_cow_fault(void *fault_addr) {
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    page_table_t *pml4 = (page_table_t *)(cr3 + _g_page_table_manager.offset);

    page_direntry_t *pte = page_table_get_pte(pml4, fault_addr);
    if (pte == NULL)
        return false;
    if (!page_direntry_get_flag(pte, PAGE_PRESENT))
        return false;
    if (!page_direntry_get_flag(pte, PAGE_COW))
        return false;

    uint64_t old_phys = page_direntry_get_address(pte) << 12;
    void *old_page = (void *)(old_phys + _g_page_table_manager.offset);

    uint16_t refcount = pfallocator_get_refcount(old_page);

    if (refcount == 1) {
        page_direntry_set_flag(pte, PAGE_READ_WRITE, true);
        page_direntry_set_flag(pte, PAGE_COW, false);
    } else {
        void *new_page = pfallocator_request_page();
        if (new_page == NULL)
            return false;

        memcpy(new_page, old_page, 0x1000);

        pfallocator_unref_page(old_page);

        uint64_t new_phys = (uint64_t)new_page - _g_page_table_manager.offset;
        page_direntry_set_address(pte, new_phys >> 12);
        page_direntry_set_flag(pte, PAGE_READ_WRITE, true);
        page_direntry_set_flag(pte, PAGE_COW, false);
    }

    __asm__ volatile("invlpg (%0)" : : "r"(fault_addr) : "memory");

    return true;
}

uint64_t virt_to_phys(void *virt) {
    return (uint64_t)virt - _g_page_table_manager.offset;
}