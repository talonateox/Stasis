#include "keyboard.h"

#include <stdbool.h>

#include "../../io/io.h"
#include "../../io/terminal.h"
#include "../pic/pic.h"

static bool shift_pressed = false;

const char ascii_table[] = {
     0 ,  0 , '1', '2',
    '3', '4', '5', '6',
    '7', '8', '9', '0',
    '-', '=',  0 ,  0 ,
    'q', 'w', 'e', 'r',
    't', 'y', 'u', 'i',
    'o', 'p', '[', ']',
     0 ,  0 , 'a', 's',
    'd', 'f', 'g', 'h',
    'j', 'k', 'l', ';',
    '\'','`',  0 , '\\',
    'z', 'x', 'c', 'v',
    'b', 'n', 'm', ',',
    '.', '/',  0 , '*',
     0 , ' '
};

char scancode_to_char(uint8_t scancode) {
    if(scancode > 58) return 0;

    return ascii_table[scancode];
}

void keyboard_handler(struct interrupt_frame* frame) {
    (void)frame;

    uint8_t scancode = inb(0x60);

    putkc(scancode_to_char(scancode));

    outb(PIC1_COMMAND, PIC_EOI);
}

void keyboard_pic_start() {
    outb(PIC1_DATA, 0b11111101);
    outb(PIC2_DATA, 0b11111111);

    while(inb(0x64) & 0x01) {
        inb(0x60);
    }

    sti();
}
