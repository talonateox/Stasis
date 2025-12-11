#ifndef TIMER_H
#define TIMER_H

#include <stdbool.h>
#include <stdint.h>

typedef void (*timer_callback_t)();

void timer_init(uint32_t frequency);
void timer_set_callback(timer_callback_t callback);
uint64_t timer_get_ticks();
void timer_sleep(uint32_t ms);

#endif
