#include "../usermode/user_syscall.h"

#define INPUT_BUFFER_SIZE 256

static char input_buffer[INPUT_BUFFER_SIZE];
static int input_index = 0;

static int streq(const char* a, const char* b) {
    while(*a && *b) {
        if(*a != *b) return 0;
        a++;
        b++;
    }
    return *a == *b;
}

static int starts_with(const char* str, const char* prefix) {
    while(*prefix) {
        if(*str != *prefix) return 0;
        str++;
        prefix++;
    }
    return 1;
}

static void process_command() {
    if(input_index > 0 && input_buffer[input_index-1] == '\n') {
        input_buffer[input_index-1] = '\0';
        input_index--;
    }

    if(streq(input_buffer, "help")) {
        print("STASIS SHELL COMMANDS:\n");
        print("    help   - show this help\n");
        print("    pid    - show current process ID\n");
        print("    echo   - echo text back\n");
        print("    clear  - clear screen (not implemented)\n");
        print("    exit   - exit shell\n");
    } else if(streq(input_buffer, "pid")) {
        print("PID: ");
        print_num(getpid());
        print("\n");
    } else if(starts_with(input_buffer, "echo ")) {
        print(input_buffer + 5);
        print("\n");
    } else if(streq(input_buffer, "exit")) {
        print("o7\n");
        exit(0);
    } else if(input_index > 0) {
        print("Unknown command: ");
        print(input_buffer);
        print("\nType 'help' for commands\n");
    }

    for(int i = 0; i < INPUT_BUFFER_SIZE; i++) {
        input_buffer[i] = 0;
    }
    input_index = 0;
}

void user_shell() {
    print("STASIS USER SHELL\n");
    print("Running in ring 3 (user mode)\n\n");

    print("shell> ");

    while(1) {
        char c;
        read(0, &c, 1);

        if(c == '\b') {
            if(input_index > 0) {
                input_index--;
                input_buffer[input_index] = '\0';
                write(1, "\b \b", 3);
            }
        } else if(c == '\n') {
            write(1, "\n", 1);
            input_buffer[input_index++] = '\n';
            process_command();
            print("shell> ");
        } else if(input_index < INPUT_BUFFER_SIZE - 1) {
            input_buffer[input_index++] = c;
            write(1, &c, 1);
        }
    }
}
