#pragma once

#include <stdint.h>

typedef struct {
    volatile uint32_t locked;
} spinlock_t;

uint64_t spin_lock(spinlock_t* lock);
void spin_unlock(spinlock_t* lock, uint64_t flags);
uint64_t spin_trylock(spinlock_t* lock, int* acquired);
