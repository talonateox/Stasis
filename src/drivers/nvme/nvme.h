#pragma once

#include <stdint.h>

typedef struct {
    void* mmio_base;
    uint32_t queue_count;
} nvme_ctrl_t;

void nvme_driver_init();