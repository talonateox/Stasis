#include "spinlock.h"

static inline uint32_t atomic_exchange(volatile uint32_t *target, uint32_t new) {
    uint32_t old;
    __asm__ volatile("lock xchg %0, %1" : "=r"(old), "+m"(*target) : "0"(new) : "memory");
    return old;
}

static inline uint64_t read_rflags() {
    uint64_t flags;
    __asm__ volatile("pushfq\n\t"
                     "pop %0"
                     : "=r"(flags)
                     :
                     : "memory");
    return flags;
}

static inline void write_rflags(uint64_t flags) {
    __asm__ volatile("push %0\n\t"
                     "popfq"
                     :
                     : "r"(flags)
                     : "memory", "cc");
}

static inline void cli_no_reorder() {
    __asm__ volatile("cli" ::: "memory");
}

static inline void cpu_pause() {
    __asm__ volatile("pause");
}

uint64_t spin_lock(spinlock_t *lock) {
    uint64_t flags = read_rflags();

    cli_no_reorder();

    while (atomic_exchange(&lock->locked, 1) == 1) {
        cpu_pause();
    }

    return flags;
}

void spin_unlock(spinlock_t *lock, uint64_t flags) {
    __asm__ volatile("" ::: "memory");
    lock->locked = 0;
    write_rflags(flags);
}

uint64_t spin_trylock(spinlock_t *lock, int *acquired) {
    uint64_t flags = read_rflags();
    cli_no_reorder();

    if (atomic_exchange(&lock->locked, 1) == 0) {
        *acquired = 1;
        return flags;
    } else {
        write_rflags(flags);
        *acquired = 0;
        return 0;
    }
}
