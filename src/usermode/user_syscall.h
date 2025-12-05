#include <stdint.h>

#include "../syscall/syscall.h"

static inline uint64_t syscall0(uint64_t num) {
    uint64_t ret;
    asm volatile(
        "syscall"
        : "=a"(ret)
        : "a"(num)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline uint64_t syscall1(uint64_t num, uint64_t arg1) {
    uint64_t ret;
    asm volatile(
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(arg1)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline uint64_t syscall2(uint64_t num, uint64_t arg1, uint64_t arg2) {
    uint64_t ret;
    asm volatile(
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(arg1), "S"(arg2)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline uint64_t syscall3(uint64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
    uint64_t ret;
    asm volatile(
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(arg1), "S"(arg2), "d"(arg3)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline void exit(int code) {
    syscall1(SYS_EXIT, code);
    __builtin_unreachable();
}

static inline int64_t write(int fd, const char* buf, uint64_t len) {
    return syscall3(SYS_WRITE, fd, (uint64_t)buf, len);
}

static inline int64_t read(int fd, char* buf, uint64_t len) {
    return syscall3(SYS_READ, fd, (uint64_t)buf, len);
}

static inline void yield() {
    syscall0(SYS_YIELD);
}

static inline void sleep(uint64_t ms) {
    syscall1(SYS_SLEEP, ms);
}

static inline uint64_t getpid() {
    return syscall0(SYS_GETPID);
}

static inline void print(const char* s) {
    uint64_t len = 0;
    while(s[len]) len++;
    write(1, s, len);
}
