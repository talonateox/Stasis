#pragma once

#include <stdint.h>

typedef struct {
    uint16_t size;
    uint64_t offset;
} __attribute__((packed)) gdt_desc_t;

typedef struct {
    uint16_t limit0;
    uint16_t base0;
    uint8_t base1;
    uint8_t access;
    uint8_t limit1_flags;
    uint8_t base2;
} __attribute__((packed)) gdt_entry_t;

typedef struct {
    uint16_t limit0;
    uint16_t base0;
    uint8_t base1;
    uint8_t access;
    uint8_t limit1_flags;
    uint8_t base2;
    uint32_t base3;
    uint32_t reserved;
} __attribute__((packed)) tss_entry_t;

typedef struct {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb_offset;
} __attribute__((packed)) tss_t;

typedef struct {
    gdt_entry_t null;
    gdt_entry_t kernel_code;
    gdt_entry_t kernel_data;
    gdt_entry_t user_data;
    gdt_entry_t user_code;
    tss_entry_t tss;
} __attribute__((packed)) __attribute__((aligned(0x1000))) gdt_t;

extern gdt_t _g_gdt;
extern tss_t _g_tss;

extern void load_gdt(gdt_desc_t* gdt_desc);

void gdt_init();
void tss_set_kernel_stack(uint64_t stack);
