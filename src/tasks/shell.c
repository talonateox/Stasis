#include "shell.h"

#include "../io/terminal.h"
#include "../drivers/keyboard/keyboard.h"
#include "../mem/alloc/page_frame_alloc.h"
#include "../drivers/acpi/acpi.h"
#include "../drivers/pci/pci.h"
#include "../task/scheduler.h"

#include "../std/string.h"

#define INPUT_BUFFER_SIZE 256

static char input_buffer[INPUT_BUFFER_SIZE];
static size_t index = 0;

void process_command() {
    if(memcmp(input_buffer, "help", 4) == 0) {
        printkf("STASIS SHELL COMMANDS:\n");
        printkf("    mem    - display memory usage\n");
        printkf("    lstask - display current running tasks\n");
        printkf("    lspci  - enumerate pci devices\n");
        printkf("    clear  - clears the screen\n");
    } else if(memcmp(input_buffer, "mem", 3) == 0) {
        printkf("FREE RAM: %k%llu%k\n", 0xcccc66, pfallocator_get_free_ram(), 0xffffff);
        printkf("USED RAM: %k%llu%k\n", 0xcccc66, pfallocator_get_used_ram(), 0xffffff);
    } else if(memcmp(input_buffer, "lstask", 6) == 0) {
        scheduler_print_tasks();
    }  else if(memcmp(input_buffer, "lspci", 5) == 0) {
        pci_enumerate(acpi_get_mcfg());
    }  else if(memcmp(input_buffer, "clear", 5) == 0) {
        terminal_clear();
    } else {
        input_buffer[index-1] = '\0';
        printkf("command '%s' not found\ntype 'help' to see a list of commands\n", input_buffer);
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
