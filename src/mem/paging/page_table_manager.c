#include "page_table_manager.h"

#include "../../std/string.h"
#include "page_map_indexer.h"
#include "../alloc/page_frame_alloc.h"

#include <stdint.h>

bool page_table_map(page_table_manager_t* manager, void* virt, void* phys) {
    page_map_indexer_t indexer = page_map_indexer_new((uint64_t)virt);

    page_direntry_t pde = manager->pml4->entries[indexer.pdp];
    page_table_t* pdp;
    if(!page_direntry_get_flag(&pde, PAGE_PRESENT)) {
        pdp = (page_table_t*)pfallocator_request_page();
        if(pdp == NULL) return false;
        memset(pdp, 0, PAGE_SIZE);

        uint64_t pdp_phys = (uint64_t)pdp - manager->offset;
        page_direntry_set_address(&pde, pdp_phys >> 12);
        page_direntry_set_flag(&pde, PAGE_PRESENT, true);
        page_direntry_set_flag(&pde, PAGE_READ_WRITE, true);
        page_direntry_set_flag(&pde, PAGE_USER_SUPER, true);
        manager->pml4->entries[indexer.pdp] = pde;
    } else {
        uint64_t pdp_phys = page_direntry_get_address(&pde) << 12;
        pdp = (page_table_t*)(pdp_phys + manager->offset);
    }

    pde = pdp->entries[indexer.pd];
    page_table_t* pd;
    if(!page_direntry_get_flag(&pde, PAGE_PRESENT)) {
        pd = (page_table_t*)pfallocator_request_page();
        if(pd == NULL) return false;
        memset(pd, 0, PAGE_SIZE);

        uint64_t pd_phys = (uint64_t)pd - manager->offset;
        page_direntry_set_address(&pde, pd_phys >> 12);
        page_direntry_set_flag(&pde, PAGE_PRESENT, true);
        page_direntry_set_flag(&pde, PAGE_READ_WRITE, true);
        page_direntry_set_flag(&pde, PAGE_USER_SUPER, true);
        pdp->entries[indexer.pd] = pde;
    } else {
        uint64_t pd_phys = page_direntry_get_address(&pde) << 12;
        pd = (page_table_t*)(pd_phys + manager->offset);
    }

    pde = pd->entries[indexer.pt];
    page_table_t* pt;
    if(!page_direntry_get_flag(&pde, PAGE_PRESENT)) {
        pt = (page_table_t*)pfallocator_request_page();
        if(pt == NULL) return false;
        memset(pt, 0, PAGE_SIZE);

        uint64_t pt_phys = (uint64_t)pt - manager->offset;
        page_direntry_set_address(&pde, pt_phys >> 12);
        page_direntry_set_flag(&pde, PAGE_PRESENT, true);
        page_direntry_set_flag(&pde, PAGE_READ_WRITE, true);
        page_direntry_set_flag(&pde, PAGE_USER_SUPER, true);
        pd->entries[indexer.pt] = pde;
    } else {
        uint64_t pt_phys = page_direntry_get_address(&pde) << 12;
        pt = (page_table_t*)(pt_phys + manager->offset);
    }

    pde.value = 0;
    page_direntry_set_address(&pde, (uint64_t)phys >> 12);
    page_direntry_set_flag(&pde, PAGE_PRESENT, true);
    page_direntry_set_flag(&pde, PAGE_READ_WRITE, true);
    page_direntry_set_flag(&pde, PAGE_USER_SUPER, true);
    pt->entries[indexer.p] = pde;

    return true;
}
