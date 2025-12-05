#pragma once

#include <stdint.h>

#define SYS_EXIT    0
#define SYS_WRITE   1
#define SYS_READ    2
#define SYS_YIELD   3
#define SYS_SLEEP   4
#define SYS_GETPID  5

void syscall_init();

uint64_t syscall_handler(uint64_t syscall, uint64_t arg1, uint64_t arg2, uint64_t arg3);
