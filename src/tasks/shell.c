#include "shell.h"

#include "../io/terminal.h"
#include "../drivers/keyboard/keyboard.h"

void shell() {
    const char* ps1 = "shell> ";
    printkf("STASIS INTERACTIVE SHELL\n\n");

    printkf("%s", ps1);

    while(1) {
        char c = keyboard_getchar();

        putkc(c);

        if(c == '\n') {
            printkf("%s", ps1);
        }
    }
}
