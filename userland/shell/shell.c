#include "../../src/fs/vfs/vfs.h"
#include "../../src/usermode/user_syscall.h"

#define INPUT_BUFFER_SIZE 256

static char input_buffer[INPUT_BUFFER_SIZE];
static int input_index = 0;
static char current_dir[256] = "/";

int strcmp(const char *s1, const char *s2) {
    const unsigned char *p1 = (const unsigned char *)s1;
    const unsigned char *p2 = (const unsigned char *)s2;

    while (*p1 != '\0' && *p1 == *p2) {
        p1++;
        p2++;
    }

    return (int)*p1 - (int)*p2;
}

void strcpy(char *s1, const char *s2) {
    while ((*s1++ = *s2++) != '\0')
        ;
}

size_t strlen(const char *str) {
    size_t length = 0;
    while (*str != '\0') {
        length++;
        str++;
    }
    return length;
}

int streq(const char *a, const char *b) {
    while (*a && *b) {
        if (*a != *b)
            return 0;
        a++;
        b++;
    }
    return *a == *b;
}

void strcat(char *dst, const char *src) {
    while (*dst)
        dst++;
    while (*src) {
        *dst++ = *src++;
    }
    *dst = '\0';
}

static void build_path(char *result, const char *path) {
    if (path[0] == '/') {
        strcpy(result, path);
    } else {
        strcpy(result, current_dir);
        if (current_dir[strlen(current_dir) - 1] != '/') {
            strcat(result, "/");
        }
        strcat(result, path);
    }
}

static void cmd_exec(const char *args) {
    if (args[0] == '\0') {
        print("exec: missing program path\n");
        return;
    }

    char arg_buf[256];
    char *argv[16];
    int argc = 0;

    strcpy(arg_buf, args);
    char *p = arg_buf;

    while (*p && argc < 15) {
        while (*p == ' ')
            p++;
        if (*p == '\0')
            break;

        argv[argc++] = p;

        while (*p && *p != ' ')
            p++;
        if (*p)
            *p++ = '\0';
    }
    argv[argc] = (char *)0;

    if (argc == 0) {
        print("exec: missing program path\n");
        return;
    }

    char path[256];
    build_path(path, argv[0]);
    argv[0] = path;

    int pid = fork();

    if (pid == 0) {
        exec(path);
        print("exec: failed to execute '");
        print(path);
        print("'\n");
        exit(1);
    } else if (pid > 0) {
        print("exec: started process ");
        print_num(pid);
        print("\n");

        int status = waitpid(pid);

        print("Process ");
        print_num(pid);
        print(" exited with code ");
        print_num(status);
        print("\n");
    } else {
        print("exec: fork failed\n");
    }
}

static void cmd_ls(const char *args) {
    char path[256];

    if (args[0] == '\0') {
        strcpy(path, current_dir);
    } else {
        build_path(path, args);
    }

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        print("ls: cannot open '");
        print(path);
        print("'\n");
        return;
    }

    char name[256];
    int count = 0;
    while (readdir(fd, name, 256) > 0) {
        print("  ");
        print(name);
        print("\n");
        count++;
    }

    close(fd);
}

static void cmd_cd(const char *args) {
    if (args[0] == '\0') {
        strcpy(current_dir, "/");
        return;
    }

    char path[256];
    build_path(path, args);

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        print("cd: no such directory: ");
        print(path);
        print("\n");
        return;
    }
    close(fd);

    strcpy(current_dir, path);
}

static void cmd_pwd(void) {
    print(current_dir);
    print("\n");
}

static void cmd_mkdir(const char *args) {
    if (args[0] == '\0') {
        print("mkdir: missing directory name\n");
        return;
    }

    char path[256];
    build_path(path, args);

    if (mkdir(path) < 0) {
        print("mkdir: failed to create '");
        print(path);
        print("'\n");
    }
}

static void cmd_touch(const char *args) {
    if (args[0] == '\0') {
        print("touch: missing file name\n");
        return;
    }

    char path[256];
    build_path(path, args);

    int fd = open(path, O_CREAT | O_RDWR);
    if (fd < 0) {
        print("touch: failed to create '");
        print(path);
        print("'\n");
        return;
    }
    close(fd);
}

static void cmd_cat(const char *args) {
    if (args[0] == '\0') {
        print("cat: missing file name\n");
        return;
    }

    char path[256];
    build_path(path, args);

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        print("cat: cannot open '");
        print(path);
        print("'\n");
        return;
    }

    char buf[128];
    int64_t n;
    while ((n = read(fd, buf, 127)) > 0) {
        buf[n] = '\0';
        print(buf);
    }

    print("\n");

    close(fd);
}

static void cmd_write(const char *args) {
    if (args[0] == '\0') {
        print("write: usage: write <file> <content>\n");
        return;
    }

    int i = 0;
    while (args[i] && args[i] != ' ')
        i++;

    if (args[i] == '\0') {
        print("write: missing content\n");
        return;
    }

    char filename[256];
    for (int j = 0; j < i; j++) {
        filename[j] = args[j];
    }
    filename[i] = '\0';

    const char *content = &args[i + 1];

    char path[256];
    build_path(path, filename);

    int fd = open(path, O_CREAT | O_WRONLY | O_TRUNC);
    if (fd < 0) {
        print("write: cannot open '");
        print(path);
        print("'\n");
        return;
    }

    write(fd, content, strlen(content));
    write(fd, "\n", 1);
    close(fd);
}

static void cmd_rm(const char *args) {
    if (args[0] == '\0') {
        print("rm: missing file name\n");
        return;
    }

    char path[256];
    build_path(path, args);

    if (unlink(path, false) < 0) {
        print("rm: failed to remove '");
        print(path);
        print("'\n");
    }
}

static void cmd_rmdir(const char *args) {
    if (args[0] == '\0') {
        print("rm: missing file name\n");
        return;
    }

    char path[256];
    build_path(path, args);

    if (unlink(path, true) < 0) {
        print("rm: failed to remove '");
        print(path);
        print("'\n");
    }
}

static void cmd_help(void) {
    print("STASIS SHELL COMMANDS:\n");
    print("  help          - show this help\n");
    print("  pid           - show current process ID\n");
    print("  echo <text>   - echo text back\n");
    print("  exit          - exit shell\n");
    print("  exec <prog>   - execute ELF binary (fork+exec)\n");
    print("  ls [path]     - list directory contents\n");
    print("  cd <path>     - change directory\n");
    print("  pwd           - print working directory\n");
    print("  mkdir <name>  - create directory\n");
    print("  touch <name>  - create empty file\n");
    print("  cat <file>    - display file contents\n");
    print("  write <f> <t> - write text to file\n");
    print("  rm <file>     - remove file\n");
    print("  rmdir <file>  - removes directory and its contents recursively\n");
}

static void process_command(void) {
    if (input_index > 0 && input_buffer[input_index - 1] == '\n') {
        input_buffer[input_index - 1] = '\0';
        input_index--;
    }

    char *cmd = input_buffer;
    while (*cmd == ' ')
        cmd++;

    if (*cmd == '\0') {
        input_index = 0;
        return;
    }

    char *args = cmd;
    while (*args && *args != ' ')
        args++;
    if (*args == ' ') {
        *args = '\0';
        args++;
        while (*args == ' ')
            args++;
    }

    if (streq(cmd, "help")) {
        cmd_help();
    } else if (streq(cmd, "pid")) {
        print("PID: ");
        print_num(getpid());
        print("\n");
    } else if (streq(cmd, "echo")) {
        print(args);
        print("\n");
    } else if (streq(cmd, "exit")) {
        print("o7\n");
        exit(0);
    } else if (streq(cmd, "exec")) {
        cmd_exec(args);
    } else if (streq(cmd, "ls")) {
        cmd_ls(args);
    } else if (streq(cmd, "cd")) {
        cmd_cd(args);
    } else if (streq(cmd, "pwd")) {
        cmd_pwd();
    } else if (streq(cmd, "mkdir")) {
        cmd_mkdir(args);
    } else if (streq(cmd, "touch")) {
        cmd_touch(args);
    } else if (streq(cmd, "cat")) {
        cmd_cat(args);
    } else if (streq(cmd, "write")) {
        cmd_write(args);
    } else if (streq(cmd, "rm")) {
        cmd_rm(args);
    } else if (streq(cmd, "rmdir")) {
        cmd_rmdir(args);
    } else {
        print("Unknown command: ");
        print(cmd);
        print("\nType 'help' for commands\n");
    }

    for (int i = 0; i < INPUT_BUFFER_SIZE; i++) {
        input_buffer[i] = 0;
    }
    input_index = 0;
}

void _start() {
    print("STASIS USER SHELL\n");
    print("Type 'help' for available commands.\n\n");

    print("shell:");
    print(current_dir);
    print("> ");

    while (1) {
        char c;
        read(0, &c, 1);

        if (c == '\b') {
            if (input_index > 0) {
                input_index--;
                input_buffer[input_index] = '\0';
                write(1, "\b \b", 3);
            }
        } else if (c == '\n') {
            write(1, "\n", 1);
            input_buffer[input_index++] = '\n';
            process_command();
            print("shell:");
            print(current_dir);
            print("> ");
        } else if (input_index < INPUT_BUFFER_SIZE - 1) {
            input_buffer[input_index++] = c;
            write(1, &c, 1);
        }
    }
}
