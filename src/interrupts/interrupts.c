#include "interrupts.h"

#include "../io/terminal.h"
#include "../mem/alloc/page_frame_alloc.h"
#include "../drivers/keyboard/keyboard.h"

__attribute__((interrupt))
void page_fault_handler(struct interrupt_frame* frame, uint64_t error_code) {
    panic_with_frame(frame, error_code, "PAGE FAULT");
}

__attribute__((interrupt))
void double_fault_handler(struct interrupt_frame* frame, uint64_t error_code) {
    panic_with_frame(frame, error_code, "DOUBLE FAULT");
}

__attribute__((interrupt))
void gp_fault_handler(struct interrupt_frame* frame, uint64_t error_code) {
    panic_with_frame(frame, error_code, "GENERAL PROTECTION FAULT");
}

idtr_t _g_idtr;

void add_idt_entry(uint64_t handler, uint64_t offset, uint8_t type_attr, uint8_t selector) {
    idt_entry_t* interrupt = (idt_entry_t*)(_g_idtr.offset + offset * sizeof(idt_entry_t));
    idt_entry_set_offset(interrupt, handler);
    interrupt->type_attr = type_attr;
    interrupt->selector = selector;
}

void interrupts_init() {
    _g_idtr.limit = 0x0fff;
    printkf_info("Allocating IDT...\n");
    _g_idtr.offset = (uint64_t)pfallocator_request_page();

    printkf_info("Preparing IDT...\n");

    add_idt_entry((uint64_t)page_fault_handler, 0x0e, IDT_INTERRUPT_GATE, 0x08);
    add_idt_entry((uint64_t)double_fault_handler, 0x08, IDT_INTERRUPT_GATE, 0x08);
    add_idt_entry((uint64_t)gp_fault_handler, 0x0d, IDT_INTERRUPT_GATE, 0x08);

    add_idt_entry((uint64_t)keyboard_handler, 0x21, IDT_INTERRUPT_GATE, 0x08);

    printkf_info("Loading IDT...\n");
    asm("lidt %0" : : "m"(_g_idtr));
    printkf_info("IDT Loaded\n");
}