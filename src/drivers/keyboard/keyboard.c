#include "keyboard.h"

#include <stdbool.h>

#include "../../io/io.h"
#include "../../io/terminal.h"
#include "../../task/task.h"
#include "../pic/pic.h"

#define KEYBOARD_BUFFER_SIZE 256

static char buffer[KEYBOARD_BUFFER_SIZE];
static size_t buffer_head = 0;
static size_t buffer_tail = 0;

static task_t *waiting_task = NULL;

static bool shift_pressed = false;

static const char scancode_to_char[128] = {
    0,   0,   '1', '2', '3', '4', '5',  '6', '7', '8', '9', '0', '-', '=', '\b', '\t', 'q', 'w', 'e',  'r', 't', 'y',
    'u', 'i', 'o', 'p', '[', ']', '\n', 0,   'a', 's', 'd', 'f', 'g', 'h', 'j',  'k',  'l', ';', '\'', '`', 0,   '\\',
    'z', 'x', 'c', 'v', 'b', 'n', 'm',  ',', '.', '/', 0,   '*', 0,   ' ', 0,    0,    0,   0,   0,    0,   0,   0,
    0,   0,   0,   0,   0,   '7', '8',  '9', '-', '4', '5', '6', '+', '1', '2',  '3',  '0', '.', 0,    0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,   0,   0,   0,   0,   0,    0,    0,   0,   0,    0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,   0,   0,   0,   0,   0,    0,    0,   0};

static const char scancode_to_char_shift[128] = {
    0,   0,   '!', '@', '#', '$', '%',  '^', '&', '*', '(', ')', '_', '+', '\b', '\t', 'Q', 'W', 'E', 'R', 'T', 'Y',
    'U', 'I', 'O', 'P', '{', '}', '\n', 0,   'A', 'S', 'D', 'F', 'G', 'H', 'J',  'K',  'L', ':', '"', '~', 0,   '|',
    'Z', 'X', 'C', 'V', 'B', 'N', 'M',  '<', '>', '?', 0,   '*', 0,   ' ', 0,    0,    0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   '7', '8',  '9', '-', '4', '5', '6', '+', '1', '2',  '3',  '0', '.', 0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,   0,   0,   0,   0,   0,    0,    0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,   0,   0,   0,   0,   0,    0,    0,   0};

static void buffer_put(char c) {
    size_t next_head = (buffer_head + 1) % KEYBOARD_BUFFER_SIZE;

    if (next_head == buffer_tail) {
        return;
    }

    buffer[buffer_head] = c;
    buffer_head = next_head;
}

static bool buffer_empty() {
    return buffer_head == buffer_tail;
}

static char buffer_get() {
    char c = buffer[buffer_tail];
    buffer_tail = (buffer_tail + 1) % KEYBOARD_BUFFER_SIZE;
    return c;
}

void keyboard_init() {
    buffer_head = 0;
    buffer_tail = 0;
    waiting_task = NULL;
    shift_pressed = false;

    while (inb(0x64) & 0x01) {
        inb(0x60);
    }
}

void keyboard_handler(struct interrupt_frame *frame) {
    (void)frame;

    uint8_t scancode = inb(0x60);

    if (scancode == 0x2A || scancode == 0x36) {
        shift_pressed = true;
    } else if (scancode == 0xAA || scancode == 0xB6) {
        shift_pressed = false;
    } else if (scancode < 0x80) {
        char c = shift_pressed ? scancode_to_char_shift[scancode] : scancode_to_char[scancode];
        if (c != 0) {
            buffer_put(c);

            if (waiting_task != NULL) {
                task_unblock(waiting_task);
                waiting_task = NULL;
            }
        }
    }

    outb(PIC1_COMMAND, PIC_EOI);
}

char keyboard_getchar() {
    while (1) {
        cli();

        if (!buffer_empty()) {
            char c = buffer_get();
            sti();
            return c;
        }

        waiting_task = task_current();

        sti();
        task_block();
    }
}

bool keyboard_haschar() {
    return !buffer_empty();
}

void keyboard_pic_start() {
    pic_set_mask(1, false);
}
