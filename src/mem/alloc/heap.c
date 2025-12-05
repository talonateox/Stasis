#include "heap.h"

#include "../paging/paging.h"
#include "page_frame_alloc.h"
#include "../../io/terminal.h"

void* heap_start;
void* heap_end;
heap_segment_hdr_t* last_hdr;
size_t heap_offset;

void heap_init(void* base, size_t page_count, size_t offset) {
    printkf_info("Initializing heap at %p...\n", base);
    void* pos = base;

    heap_offset = offset;

    for(size_t i = 0; i < page_count; i++) {
        void* virt_page = pfallocator_request_page();
        void* phys_page = (void*)((uint64_t)virt_page - offset);
        page_map_memory(pos, phys_page);
        pos = (void*)((size_t)pos + PAGE_SIZE);
    }

    size_t heap_length = page_count * PAGE_SIZE;

    heap_start = base;
    heap_end = (void*)((size_t)heap_start + heap_length);

    heap_segment_hdr_t* start_segment = (heap_segment_hdr_t*)base;
    start_segment->length = heap_length - sizeof(heap_segment_hdr_t);
    start_segment->next = NULL;
    start_segment->last = NULL;
    start_segment->free = true;
    last_hdr = start_segment;
    printkf_ok("Initialized heap at %p-%p\n", heap_start, heap_end);
}

void heap_expand(size_t size) {
    if(size % PAGE_SIZE) {
        size -= size % PAGE_SIZE;
        size += PAGE_SIZE;
    }

    size_t page_count = size / PAGE_SIZE;
    heap_segment_hdr_t* new_seg = (heap_segment_hdr_t*)heap_end;

    for(size_t i = 0; i < page_count; i++) {
         void* page = pfallocator_request_page();
         if(page == NULL) {
             panic("HEAP: OUT OF MEMORY");
         }
         void* phys_page = (void*)((uint64_t)page - heap_offset);
         page_map_memory(heap_end, phys_page);
         heap_end = (void*)((size_t)heap_end + PAGE_SIZE);
     }

    new_seg->free = true;
    new_seg->last = last_hdr;
    last_hdr->next = new_seg;
    last_hdr = new_seg;
    new_seg->next = NULL;
    new_seg->length = size - sizeof(heap_segment_hdr_t);
    heap_segment_combine_backward(new_seg);
}

void* malloc(size_t size) {
    if(size % 0x10 > 0) {
        size -= (size % 0x10);
        size += 0x10;
    }

    if(size == 0) return NULL;

    heap_segment_hdr_t* curr = (heap_segment_hdr_t*)heap_start;
    while(true) {
        if(curr->free) {
            if(curr->length >= size) {
                if(curr->length > size + sizeof(heap_segment_hdr_t) + 0x10) {
                    heap_segment_split(curr, size);
                }
                curr->free = false;
                return (void*)((uint64_t)curr + sizeof(heap_segment_hdr_t));
            }
        }
        if(curr->next == NULL) break;
        curr = curr->next;
    }
    heap_expand(size);
    return malloc(size);
}

void free(void* address) {
    heap_segment_hdr_t* seg = (heap_segment_hdr_t*)((uint64_t)address - sizeof(heap_segment_hdr_t));

    if(seg->free) {
        printkf_error("free: double free detected at %p\n", address);
        return;
    }

    seg->free = true;
    heap_segment_combine_forward(seg);
    heap_segment_combine_backward(seg);
}

void heap_segment_combine_forward(heap_segment_hdr_t* hdr) {
    if(hdr->next == NULL) return;
    if(!hdr->next->free) return;

    if(hdr->next == last_hdr) last_hdr = hdr;
    if(hdr->next->next != NULL) hdr->next->next->last = hdr;

    hdr->length = hdr->length + hdr->next->length + sizeof(heap_segment_hdr_t);
    hdr->next = hdr->next->next;
}

void heap_segment_combine_backward(heap_segment_hdr_t* hdr) {
    if(hdr->last != NULL && hdr->last->free) heap_segment_combine_forward(hdr->last);
}

heap_segment_hdr_t* heap_segment_split(heap_segment_hdr_t* hdr, size_t length) {
    if(length < 0x10) return NULL;

    size_t split_length = hdr->length - length - sizeof(heap_segment_hdr_t);
    if(split_length < 0x10) return NULL;

    heap_segment_hdr_t* new_seg = (heap_segment_hdr_t*)((uint64_t)hdr + sizeof(heap_segment_hdr_t) + length);

    new_seg->length = split_length;
    new_seg->free = hdr->free;
    new_seg->last = hdr;
    new_seg->next = hdr->next;

    if(hdr->next != NULL) {
        hdr->next->last = new_seg;
    }
    hdr->next = new_seg;
    hdr->length = length;

    if(last_hdr == hdr) last_hdr = new_seg;

    return new_seg;
}
