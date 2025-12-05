#include "pic.h"

#include <stdint.h>
#include "../../io/io.h"
#include "../../io/terminal.h"

void pic_remap() {
    printkf_info("Remapping PIC...\n");
    uint8_t a1, a2;
    a1 = inb(PIC1_DATA);
    a2 = inb(PIC2_DATA);

    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);

    outb(PIC1_DATA, 0x20);
    outb(PIC2_DATA, 0x28);

    outb(PIC1_DATA, 4);
    outb(PIC2_DATA, 2);

    outb(PIC1_DATA, ICW4_8086);
    outb(PIC2_DATA, ICW4_8086);

    outb(PIC1_DATA, a1);
    outb(PIC2_DATA, a2);
    printkf_ok("PIC Remapped\n");
}

void pic_set_mask(uint8_t irq, bool masked) {
    uint16_t port;
    uint8_t value;

    if(irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }

    if(masked) {
        value = inb(port) | (1 << irq);
    } else {
        value = inb(port) & ~(1 << irq);
    }

    outb(port, value);
}
