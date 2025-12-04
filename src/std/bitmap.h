#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    size_t size;
    uint8_t* buffer;
} bitmap_t;

bool bitmap_get(bitmap_t* bitmap, uint64_t index);
void bitmap_set(bitmap_t* bitmap, uint64_t index, bool value);