#include "syscall.h"

#include "../task/task.h"
#include "../io/terminal.h"
#include "../drivers/keyboard/keyboard.h"

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

uint64_t syscall_handler(uint64_t syscall, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
    switch(syscall) {
        case SYS_EXIT: {
            task_exit();
            return 0;
        }
        case SYS_WRITE: {
            for(uint64_t i = 0; i < arg3; i++) {
                char c = ((char*)arg2)[i];
                putkc(c);
            }
            return arg3;
        }
        case SYS_READ: {
            char* buf = (char*)arg2;
            uint64_t i = 0;
            while(i < arg3) {
                char c = keyboard_getchar();
                buf[i++] = c;
                if(c == '\n') break;
            }
            return i;
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
        default: {
            printkf_error("syscall_handler(): unknown syscall: %llu\n", syscall);
            return -1;
        }
    }
}
