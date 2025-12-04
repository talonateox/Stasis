#pragma once

#include <stdint.h>
#include "../../interrupts/interrupts.h"

__attribute__((interrupt)) void keyboard_handler(struct interrupt_frame* frame);

void keyboard_pic_start();