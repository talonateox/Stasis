#include "timer.h"

#include "pit.h"

static uint32_t timer_frequency = 0;

void timer_init(uint32_t frequency) {
    timer_frequency = frequency;
    pit_init(frequency);
}

void timer_set_callback(timer_callback_t callback) {
    pit_set_callback(callback);
}

uint64_t timer_get_ticks() {
    return pit_get_ticks();
}

void timer_sleep(uint32_t ms) {
    if (timer_frequency == 0)
        return;

    uint64_t ticks_to_wait = (ms * timer_frequency) / 1000;
    uint64_t start_tick = timer_get_ticks();

    while (timer_get_ticks() - start_tick < ticks_to_wait) {
        __asm__ volatile("hlt");
    }
}
