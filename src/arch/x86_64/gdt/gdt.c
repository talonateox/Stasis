#include "gdt.h"
#include "gdt.h"

tss_t _g_tss = {0};

__attribute__((aligned(0x1000)))
gdt_t _g_gdt = {
    .null = {0, 0, 0, 0x00, 0x00, 0},
    .kernel_code = {0, 0, 0, 0x9A, 0xA0, 0},
    .kernel_data = {0, 0, 0, 0x92, 0xA0, 0},
    .user_data = {0, 0, 0, 0xF2, 0xA0, 0},
    .user_code = {0, 0, 0, 0xFA, 0xA0, 0},
    .tss = {0, 0, 0, 0x89, 0x00, 0, 0, 0},
};

static void setup_tss_descriptor(void) {
    uint64_t tss_addr = (uint64_t)&_g_tss;
    uint32_t tss_size = sizeof(tss_t) - 1;

    _g_gdt.tss.limit0 = tss_size & 0xFFFF;
    _g_gdt.tss.base0 = tss_addr & 0xFFFF;
    _g_gdt.tss.base1 = (tss_addr >> 16) & 0xFF;
    _g_gdt.tss.access = 0x89;
    _g_gdt.tss.limit1_flags = ((tss_size >> 16) & 0x0F);
    _g_gdt.tss.base2 = (tss_addr >> 24) & 0xFF;
    _g_gdt.tss.base3 = (tss_addr >> 32) & 0xFFFFFFFF;
    _g_gdt.tss.reserved = 0;
}

void gdt_init(void) {
    _g_tss.reserved0 = 0;
    _g_tss.rsp0 = 0;
    _g_tss.rsp1 = 0;
    _g_tss.rsp2 = 0;
    _g_tss.reserved1 = 0;
    _g_tss.ist1 = 0;
    _g_tss.ist2 = 0;
    _g_tss.ist3 = 0;
    _g_tss.ist4 = 0;
    _g_tss.ist5 = 0;
    _g_tss.ist6 = 0;
    _g_tss.ist7 = 0;
    _g_tss.reserved2 = 0;
    _g_tss.reserved3 = 0;
    _g_tss.iopb_offset = sizeof(tss_t);

    setup_tss_descriptor();

    gdt_desc_t desc;
    desc.size = sizeof(gdt_t) - 1;
    desc.offset = (uint64_t)&_g_gdt;
    load_gdt(&desc);

    asm volatile("ltr %w0" : : "r"((uint16_t)GDT_TSS));
}

void tss_set_kernel_stack(uint64_t stack) {
    _g_tss.rsp0 = stack;
}
