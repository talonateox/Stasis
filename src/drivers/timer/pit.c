#include "pit.h"

#include <stddef.h>
#include "../../io/io.h"
#include "../pic/pic.h"
#include "../../io/terminal.h"

static uint64_t pit_ticks = 0;
static void (*pit_callback)() = NULL;

void pit_init(uint32_t frequency) {
    uint32_t divisor = PIT_BASE_FREQ / frequency;

    if(divisor > 65535) divisor = 65535;
    if(divisor < 1) divisor = 1;

    outb(PIT_COMMAND, PIT_CMD_CHANNEL0 | PIT_CMD_LSB_MSB | PIT_CMD_MODE3 | PIT_CMD_BINARY);

    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));

    pit_ticks = 0;
    pic_set_mask(0, false);
}

void pit_set_callback(void (*callback)()) {
    pit_callback = callback;
}

uint64_t pit_get_ticks() {
    return pit_ticks;
}

void pit_interrupt_handler() {
    pit_ticks++;

    if(pit_callback != NULL) {
        pit_callback();
    }
}
