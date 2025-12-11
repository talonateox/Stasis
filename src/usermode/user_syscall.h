#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../syscall/syscall.h"

static inline uint64_t syscall0(uint64_t num) {
    uint64_t ret;
    __asm__ volatile("syscall" : "=a"(ret) : "a"(num) : "rcx", "r11", "memory");
    return ret;
}

static inline uint64_t syscall1(uint64_t num, uint64_t arg1) {
    uint64_t ret;
    __asm__ volatile("syscall"
                     : "=a"(ret)
                     : "a"(num), "D"(arg1)
                     : "rcx", "r11", "memory");
    return ret;
}

static inline uint64_t syscall2(uint64_t num, uint64_t arg1, uint64_t arg2) {
    uint64_t ret;
    __asm__ volatile("syscall"
                     : "=a"(ret)
                     : "a"(num), "D"(arg1), "S"(arg2)
                     : "rcx", "r11", "memory");
    return ret;
}

static inline uint64_t syscall3(uint64_t num, uint64_t arg1, uint64_t arg2,
                                uint64_t arg3) {
    uint64_t ret;
    __asm__ volatile("syscall"
                     : "=a"(ret)
                     : "a"(num), "D"(arg1), "S"(arg2), "d"(arg3)
                     : "rcx", "r11", "memory");
    return ret;
}

static inline void exit(int code) {
    syscall1(SYS_EXIT, code);
    __builtin_unreachable();
}

static inline void yield() { syscall0(SYS_YIELD); }

static inline void sleep(uint64_t ms) { syscall1(SYS_SLEEP, ms); }

static inline uint64_t getpid() { return syscall0(SYS_GETPID); }

static inline int exec(const char* path) {
    return syscall1(SYS_EXEC, (uint64_t)path);
}

static inline int fork() { return (int)syscall0(SYS_FORK); }

static inline int waitpid(int pid) { return (int)syscall1(SYS_WAITPID, pid); }

static inline int64_t write(int fd, const void* buf, size_t len) {
    return syscall3(SYS_WRITE, fd, (uint64_t)buf, len);
}

static inline int64_t read(int fd, void* buf, size_t len) {
    return syscall3(SYS_READ, fd, (uint64_t)buf, len);
}

static inline int open(const char* path, int flags) {
    return syscall2(SYS_OPEN, (uint64_t)path, flags);
}

static inline int close(int fd) { return syscall1(SYS_CLOSE, fd); }

static inline int64_t seek(int fd, int64_t offset, int whence) {
    return syscall3(SYS_SEEK, fd, offset, whence);
}

static inline int mkdir(const char* path) {
    return syscall1(SYS_MKDIR, (uint64_t)path);
}

static inline int readdir(int fd, char* name, size_t size) {
    return syscall3(SYS_READDIR, fd, (uint64_t)name, size);
}

static inline int unlink(const char* path, bool recursive) {
    return syscall2(SYS_UNLINK, (uint64_t)path, recursive);
}

static inline void print(const char* s) {
    uint64_t len = 0;
    while (s[len]) len++;
    write(1, s, len);
}

static inline void print_num(int64_t n) {
    char buf[21];
    int i = 0;
    int neg = 0;

    if (n < 0) {
        neg = 1;
        n = -n;
    }

    if (n == 0) {
        write(1, "0", 1);
        return;
    }

    while (n > 0) {
        buf[i++] = '0' + (n % 10);
        n /= 10;
    }

    if (neg) write(1, "-", 1);

    while (i > 0) {
        write(1, &buf[--i], 1);
    }
}
