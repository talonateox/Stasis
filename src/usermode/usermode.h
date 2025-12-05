#pragma once

#include <stdint.h>

void jump_to_usermode(uint64_t user_rip, uint64_t user_rsp);
