#pragma once

#include <stdint.h>

#define NVME_ADMIN_IDENTIFY 0x06
#define NVME_ADMIN_CREATE_IO_CQ 0x05
#define NVME_ADMIN_CREATE_IO_SQ 0x01
#define NVME_CMD_READ 0x02
#define NVME_CMD_WRITE 0x01

typedef struct {
    uint64_t cap;
    uint32_t vs;
    uint32_t intms;
    uint32_t intmc;
    uint32_t cc;
    uint32_t reserved1;
    uint32_t csts;
    uint32_t nssr;
    uint32_t aqa;
    uint64_t asq;
    uint64_t acq;
} __attribute__((packed)) nvme_bar_t;

typedef struct {
    uint32_t cdw0;
    uint32_t nsid;
    uint64_t reserved;
    uint64_t mptr;
    uint64_t prp1;
    uint64_t prp2;
    uint32_t cdw10;
    uint32_t cdw11;
    uint32_t cdw12;
    uint32_t cdw13;
    uint32_t cdw14;
    uint32_t cdw15;
} __attribute__((packed)) nvme_sqe_t;

typedef struct {
    uint32_t dw0;
    uint32_t dw1;
    uint16_t sq_head;
    uint16_t sq_id;
    uint16_t cid;
    uint16_t status;
} __attribute__((packed)) nvme_cqe_t;

typedef struct {
    nvme_sqe_t* sq;
    nvme_cqe_t* cq;
    uint32_t* sq_doorbell;
    uint32_t* cq_doorbell;
    uint16_t sq_tail;
    uint16_t cq_head;
    uint16_t phase;
    uint16_t size;
} nvme_queue_t;

typedef struct {
    volatile nvme_bar_t* regs;
    void* mmio_base;

    nvme_queue_t admin_queue;
    nvme_queue_t* io_queues;
    uint32_t num_io_queues;

    uint32_t max_queue_entries;
    uint32_t doorbell_stride;

    uint64_t num_namespaces;
    uint64_t block_size;
    uint64_t num_blocks;
} nvme_ctrl_t;

void nvme_driver_init();
int nvme_read(nvme_ctrl_t* ctrl, uint64_t lba, uint32_t num_blocks,
              void* buffer);
int nvme_write(nvme_ctrl_t* ctrl, uint64_t lba, uint32_t num_blocks,
               const void* buffer);
void nvme_register_device(nvme_ctrl_t* ctrl);