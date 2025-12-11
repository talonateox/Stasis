#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef struct heap_segment_hdr {
    size_t length;
    struct heap_segment_hdr* next;
    struct heap_segment_hdr* last;
    bool free;
} heap_segment_hdr_t;

void heap_init(void* base, size_t page_count, size_t offset);
void heap_expand(size_t size);

void* malloc(size_t size);
void free(void* address);

void heap_segment_combine_forward(heap_segment_hdr_t* hdr);
void heap_segment_combine_backward(heap_segment_hdr_t* hdr);
heap_segment_hdr_t* heap_segment_split(heap_segment_hdr_t* hdr, size_t length);
