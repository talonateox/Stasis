#include "shell.h"

#include "../io/terminal.h"
#include "../drivers/keyboard/keyboard.h"
#include "../std/string.h"

#define INPUT_BUFFER_SIZE 256

static char input_buffer[INPUT_BUFFER_SIZE];
static size_t index = 0;

void process_command() {
    if(memcmp(input_buffer, "test", 4) == 0) {
        printkf("test command executed!\n");
    } else {
        input_buffer[index-1] = '\0';
        printkf("Command '%s' not found\n", input_buffer);
    }
    memset(input_buffer, 0, index);
    index = 0;
}

void shell() {
    const char* ps1 = "shell> ";
    printkf("STASIS INTERACTIVE SHELL\n\n");

    printkf("%k%s%r", 0x89cff0, ps1);

    while(1) {
        char c = keyboard_getchar();

        putkc(c);
        input_buffer[index++] = c;

        if(c == '\n') {
            process_command();
            printkf("%k%s%r", 0x89cff0, ps1);
        }
    }
}
