#ifndef PIT_H
#define PIT_H

#include <stdint.h>
#include <stdbool.h>

#include "../../interrupts/interrupts.h"

#define PIT_CHANNEL0    0x40
#define PIT_CHANNEL1    0x41
#define PIT_CHANNEL2    0x42
#define PIT_COMMAND     0x43

#define PIT_BASE_FREQ   1193182

#define PIT_CMD_BINARY      0x00
#define PIT_CMD_MODE2       0x04
#define PIT_CMD_MODE3       0x06
#define PIT_CMD_LATCH       0x00
#define PIT_CMD_LSB_MSB     0x30
#define PIT_CMD_CHANNEL0    0x00

void pit_init(uint32_t frequency);
void pit_set_callback(void (*callback)());
uint64_t pit_get_ticks();
void pit_interrupt_handler();

#endif
