#pragma once

#include <stdint.h>

#define IDT_TRAP_GATE 0b10001111
#define IDT_INTERRUPT_GATE 0b10001110
#define IDT_CALL_GATE 0b10001100

typedef struct {
    uint16_t offset0;
    uint16_t selector;
    uint8_t ist;
    uint8_t type_attr;
    uint16_t offset1;
    uint32_t offset2;
    uint32_t ignore;
} idt_entry_t;

typedef struct {
    uint16_t limit;
    uint64_t offset;
} __attribute__((packed)) idtr_t;

void idt_entry_set_offset(idt_entry_t* entry, uint64_t offset);
uint64_t idt_entry_get_offset(idt_entry_t* entry);