#include "gdt.h"

__attribute__((aligned(0x1000)))
gdt_t _g_gdt = {
    .null = {0, 0, 0, 0x00, 0x00, 0},
    .kernel_code = {0, 0, 0, 0x9a, 0xa0, 0},
    .kernel_data = {0, 0, 0, 0x92, 0xa0, 0},
    .user_null = {0, 0, 0, 0x00, 0x00, 0},
    .user_code = {0, 0, 0, 0x9a, 0xa0, 0},
    .user_data = {0, 0, 0, 0x92, 0xa0, 0},
};

void gdt_init() {
    gdt_desc_t desc;
    desc.size = sizeof(gdt_t) - 1;
    desc.offset = (uint64_t)&_g_gdt;
    load_gdt(&desc);
}