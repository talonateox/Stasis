#pragma once

#include <stdint.h>

#define SYS_EXIT 0
#define SYS_WRITE 1
#define SYS_READ 2
#define SYS_YIELD 3
#define SYS_SLEEP 4
#define SYS_GETPID 5
#define SYS_EXEC 6
#define SYS_FORK 7
#define SYS_WAITPID 8

#define SYS_OPEN 10
#define SYS_CLOSE 11
#define SYS_SEEK 12
#define SYS_MKDIR 13
#define SYS_READDIR 14
#define SYS_STAT 15
#define SYS_UNLINK 16

void syscall_init();

uint64_t syscall_handler(uint64_t syscall, uint64_t arg1, uint64_t arg2, uint64_t arg3);
