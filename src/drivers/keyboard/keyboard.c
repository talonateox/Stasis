#include "keyboard.h"

#include <stdbool.h>

#include "../../io/io.h"
#include "../../io/terminal.h"
#include "../pic/pic.h"

static bool shift_pressed = false;

static const char scancode_to_char[128] = {
    0,    0,   '1',  '2',  '3',  '4',  '5',  '6',
    '7',  '8', '9',  '0',  '-',  '=',  '\b', '\t',
    'q',  'w', 'e',  'r',  't',  'y',  'u',  'i',
    'o',  'p', '[',  ']',  '\n', 0,    'a',  's',
    'd',  'f', 'g',  'h',  'j',  'k',  'l',  ';',
    '\'', '`', 0,    '\\', 'z',  'x',  'c',  'v',
    'b',  'n', 'm',  ',',  '.',  '/',  0,    '*',
    0,    ' ', 0,    0,    0,    0,    0,    0,
    0,    0,   0,    0,    0,    0,    0,    '7',
    '8',  '9', '-',  '4',  '5',  '6',  '+',  '1',
    '2',  '3', '0',  '.',  0,    0,    0,    0,
    0,    0,   0,    0,    0,    0,    0,    0,
    0,    0,   0,    0,    0,    0,    0,    0,
    0,    0,   0,    0,    0,    0,    0,    0,
    0,    0,   0,    0,    0,    0,    0,    0,
    0,    0,   0,    0,    0,    0,    0,    0
};

static const char scancode_to_char_shift[128] = {
    0,    0,   '!',  '@',  '#',  '$',  '%',  '^',
    '&',  '*', '(',  ')',  '_',  '+',  '\b', '\t',
    'Q',  'W', 'E',  'R',  'T',  'Y',  'U',  'I',
    'O',  'P', '{',  '}',  '\n', 0,    'A',  'S',
    'D',  'F', 'G',  'H',  'J',  'K',  'L',  ':',
    '"',  '~', 0,    '|',  'Z',  'X',  'C',  'V',
    'B',  'N', 'M',  '<',  '>',  '?',  0,    '*',
    0,    ' ', 0,    0,    0,    0,    0,    0,
    0,    0,   0,    0,    0,    0,    0,    '7',
    '8',  '9', '-',  '4',  '5',  '6',  '+',  '1',
    '2',  '3', '0',  '.',  0,    0,    0,    0,
    0,    0,   0,    0,    0,    0,    0,    0,
    0,    0,   0,    0,    0,    0,    0,    0,
    0,    0,   0,    0,    0,    0,    0,    0,
    0,    0,   0,    0,    0,    0,    0,    0,
    0,    0,   0,    0,    0,    0,    0,    0
};

void keyboard_handler(struct interrupt_frame* frame) {
    (void)frame;

    uint8_t scancode = inb(0x60);

    if(scancode == 0x2A || scancode == 0x36) {
        shift_pressed = true;
    } else if(scancode == 0xAA || scancode == 0xB6) {
        shift_pressed = false;
    } else if(scancode < 0x80) {
        char c = shift_pressed ? scancode_to_char_shift[scancode] : scancode_to_char[scancode];
        if(c != 0) {
            putkc(c);
        }
    }

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
