#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../../interrupts/interrupts.h"

void keyboard_init();

__attribute__((interrupt)) void keyboard_handler(struct interrupt_frame* frame);

char keyboard_getchar();
bool keyboard_haschar();

void keyboard_pic_start();
