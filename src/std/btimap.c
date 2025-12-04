#include "bitmap.h"

bool bitmap_get(bitmap_t* bitmap, uint64_t index) {
    uint8_t bit = 0b10000000 >> (index % 8);
    return (bitmap->buffer[index / 8] & bit) > 0;
}

void bitmap_set(bitmap_t* bitmap, uint64_t index, bool value) {
    uint64_t byte = index / 8;
    uint8_t bit = 0b10000000 >> (index % 8);
    bitmap->buffer[byte] &= ~bit;
    if(value) {
        bitmap->buffer[byte] |= bit;
    }
}
