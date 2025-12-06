#include "syscall.h"

#include "../task/task.h"
#include "../io/terminal.h"
#include "../drivers/keyboard/keyboard.h"
#include "../fs/vfs/vfs.h"
#include "../elf/elf.h"
#include "../mem/alloc/heap.h"
#include "../usermode/usermode.h"

#define MSR_EFER   0xC0000080
#define MSR_STAR   0xC0000081
#define MSR_LSTAR  0xC0000082
#define MSR_SFMASK 0xC0000084

#define EFER_SCE   (1 << 0)

extern void syscall_entry();

static inline void wrmsr(uint32_t msr, uint64_t value) {
    uint32_t low = value & 0xffffffff;
    uint32_t high = value >> 32;
    asm volatile("wrmsr" : : "c"(msr), "a"(low), "d"(high));
}

static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t low, high;
    asm volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

void syscall_init() {
    printkf_info("Enabling syscalls...\n");
    uint64_t efer = rdmsr(MSR_EFER);
    efer |= EFER_SCE;
    wrmsr(MSR_EFER, efer);

    uint64_t star = 0;
    star |= ((uint64_t)0x10 << 48);
    star |= ((uint64_t)0x08 << 32);
    wrmsr(MSR_STAR, star);

    wrmsr(MSR_LSTAR, (uint64_t)syscall_entry);

    wrmsr(MSR_SFMASK, 0x200);

    printkf_ok("Syscalls enabled\n");
}

static int sys_exec(const char* path) {
    int fd = vfs_open(path, O_RDONLY);
    if (fd < 0) {
        printkf_error("exec: failed to open '%s'\n", path);
        return -1;
    }

    int64_t size = vfs_seek(fd, 0, SEEK_END);
    if (size <= 0) {
        vfs_close(fd);
        printkf_error("exec: failed to get size of '%s'\n", path);
        return -1;
    }
    vfs_seek(fd, 0, SEEK_SET);

    void* elf_data = malloc(size);
    if (elf_data == NULL) {
        vfs_close(fd);
        printkf_error("exec: out of memory\n");
        return -1;
    }

    int64_t bytes_read = vfs_read(fd, elf_data, size);
    vfs_close(fd);

    if (bytes_read != size) {
        free(elf_data);
        printkf_error("exec: failed to read '%s'\n", path);
        return -1;
    }

    uint64_t entry_point = 0;
    if (elf_load(elf_data, size, &entry_point) < 0) {
        free(elf_data);
        printkf_error("exec: failed to load '%s'\n", path);
        return -1;
    }

    free(elf_data);

    task_t* current = task_current();
    if (current == NULL) {
        printkf_error("exec: no current task\n");
        return -1;
    }

    current->entry_point = (void(*)())entry_point;

    uint64_t user_rsp = (uint64_t)current->user_stack + current->user_stack_size;
    user_rsp &= ~0xFULL;

    printkf_ok("exec: jumping to 0x%llx with stack 0x%llx\n", entry_point, user_rsp);

    jump_to_usermode(entry_point, user_rsp);

    return 0;
}

uint64_t syscall_handler(uint64_t syscall, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
    switch(syscall) {
        case SYS_EXIT: {
            task_exit();
            return 0;
        }
        case SYS_YIELD: {
            task_yield();
            return 0;
        }
        case SYS_SLEEP: {
            sleep_ms(arg1);
            return 0;
        }
        case SYS_GETPID: {
            task_t* current = task_current();
            return current ? current->pid : 0;
        }
        case SYS_EXEC: {
            const char* path = (const char*)arg1;
            return sys_exec(path);
        }
        case SYS_FORK: {
            task_t* child = task_fork();
            return child ? child->pid : -1;
        }
        case SYS_WAITPID: {
            uint32_t pid = (uint32_t)arg1;
            return task_waitpid(pid);
        }
        case SYS_WRITE: {
            int fd = (int)arg1;
            const char* buf = (const char*)arg2;
            size_t size = (size_t)arg3;

            if (fd == 1 || fd == 2) {
                for (size_t i = 0; i < size; i++) {
                    putkc(buf[i]);
                }
                return size;
            }

            return vfs_write(fd, buf, size);
        }
        case SYS_READ: {
            int fd = (int)arg1;
            char* buf = (char*)arg2;
            size_t size = (size_t)arg3;

            if (fd == 0) {
                size_t i = 0;
                while (i < size) {
                    char c = keyboard_getchar();
                    buf[i++] = c;
                    if (c == '\n') break;
                }
                return i;
            }

            return vfs_read(fd, buf, size);
        }
        case SYS_OPEN: {
            const char* path = (const char*)arg1;
            int flags = (int)arg2;
            return vfs_open(path, flags);
        }
        case SYS_CLOSE: {
            int fd = (int)arg1;
            return vfs_close(fd);
        }
        case SYS_SEEK: {
            int fd = (int)arg1;
            int64_t offset = (int64_t)arg2;
            int whence = (int)arg3;
            return vfs_seek(fd, offset, whence);
        }
        case SYS_MKDIR: {
            const char* path = (const char*)arg1;
            return vfs_mkdir(path);
        }
        case SYS_READDIR: {
            int fd = (int)arg1;
            char* name = (char*)arg2;
            size_t size = (size_t)arg3;
            return vfs_readdir(fd, name, size);
        }
        case SYS_UNLINK: {
            const char* path = (const char*)arg1;
            return vfs_unlink(path);
        }
        default: {
            printkf_error("syscall_handler(): unknown syscall: %llu\n", syscall);
            return -1;
        }
    }
}
