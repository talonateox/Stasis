#include "nvme.h"

#include "../../io/terminal.h"
#include "../../mem/alloc/heap.h"
#include "../../mem/alloc/page_frame_alloc.h"
#include "../../mem/paging/paging.h"
#include "../../std/string.h"
#include "../pci/pci.h"

static const pci_device_id_t nvme_ids[] = {
    {PCI_DEVICE_CLASS(0x010802, 0xffffff)},
    {0},
};

static int nvme_submit_command(nvme_queue_t *queue, nvme_sqe_t *cmd, void *result) {
    uint16_t tail = queue->sq_tail;

    memcpy(&queue->sq[tail], cmd, sizeof(nvme_sqe_t));

    queue->sq[tail].cdw0 = (queue->sq[tail].cdw0 & ~0xFFFF0000) | (tail << 16);
    queue->sq_tail = (tail + 1) % queue->size;

    *queue->sq_doorbell = queue->sq_tail;

    uint16_t cid = tail;
    uint32_t timeout = 1000000;

    while (timeout--) {
        __asm__ volatile("mfence" ::: "memory");

        nvme_cqe_t *cqe = &queue->cq[queue->cq_head];

        uint16_t phase = (cqe->status >> 0) & 1;
        if (phase != queue->phase) {
            continue;
        }

        uint16_t completion_cid = cqe->cid;
        uint16_t status = (cqe->status >> 1) & 0x7FF;

        if (completion_cid == cid) {
            if (result) {
                memcpy(result, cqe, sizeof(nvme_cqe_t));
            }

            queue->cq_head = (queue->cq_head + 1) % queue->size;
            if (queue->cq_head == 0) {
                queue->phase = !queue->phase;
            }
            *queue->cq_doorbell = queue->cq_head;

            if (status != 0) {
                printkf_error("nvme_submit_command(): Command %u failed with status 0x%x\n", cid, status);
            }
            return status == 0 ? 0 : -1;
        }

        printkf_info("nvme_submit_command(): Got completion for CID %u while waiting for %u\n", completion_cid, cid);
        queue->cq_head = (queue->cq_head + 1) % queue->size;
        if (queue->cq_head == 0) {
            queue->phase = !queue->phase;
        }
        *queue->cq_doorbell = queue->cq_head;
    }

    printkf_error("nvme_submit_command(): Command %u timeout (no completion after %u iterations)\n", cid, 5000000);
    return -1;
}

static int nvme_reset_controller(nvme_ctrl_t *ctrl) {
    ctrl->regs->cc = 0;

    uint64_t cap = ctrl->regs->cap;
    uint8_t cap_to = (cap >> 24) & 0xFF;
    uint32_t timeout_ms = cap_to * 500;
    uint32_t timeout = timeout_ms * 1000;

    while ((ctrl->regs->csts & 0x1) && timeout--) {
        for (volatile int i = 0; i < 1000; i++)
            ;
    }

    uint32_t csts = ctrl->regs->csts;

    if (csts & 0x1) {
        printkf_error("nvme_reset_controller(): Controller failed to reset (CSTS.RDY "
                      "still 1)\n");
        return -1;
    }

    return 0;
}

static int nvme_create_admin_queue(nvme_ctrl_t *ctrl) {
    uint16_t queue_size = 64;

    size_t sq_size = queue_size * sizeof(nvme_sqe_t);
    size_t cq_size = queue_size * sizeof(nvme_cqe_t);

    ctrl->admin_queue.sq = (nvme_sqe_t *)pfallocator_request_page();
    ctrl->admin_queue.cq = (nvme_cqe_t *)pfallocator_request_page();

    if (!ctrl->admin_queue.sq || !ctrl->admin_queue.cq) {
        printkf_error("nvme_create_admin_queue(): Failed to allocate queue memory\n");
        return -1;
    }

    memset(ctrl->admin_queue.sq, 0, sq_size);
    memset(ctrl->admin_queue.cq, 0, cq_size);

    ctrl->admin_queue.size = queue_size;
    ctrl->admin_queue.sq_tail = 0;
    ctrl->admin_queue.cq_head = 0;
    ctrl->admin_queue.phase = 1;

    uint64_t sq_phys = virt_to_phys(ctrl->admin_queue.sq);
    uint64_t cq_phys = virt_to_phys(ctrl->admin_queue.cq);

    if (sq_phys & 0xFFF) {
        printkf_error("nvme_create_admin_queue(): SQ not page-aligned!\n");
        return -1;
    }
    if (cq_phys & 0xFFF) {
        printkf_error("nvme_create_admin_queue(): CQ not page-aligned!\n");
        return -1;
    }

    ctrl->regs->aqa = ((queue_size - 1) << 16) | (queue_size - 1);
    ctrl->regs->asq = sq_phys;
    ctrl->regs->acq = cq_phys;

    ctrl->admin_queue.sq_doorbell = (uint32_t *)((uint8_t *)ctrl->regs + 0x1000);
    ctrl->admin_queue.cq_doorbell = (uint32_t *)((uint8_t *)ctrl->regs + 0x1000 + ctrl->doorbell_stride);
    return 0;
}

static int nvme_enable_controller(nvme_ctrl_t *ctrl) {
    uint32_t csts = ctrl->regs->csts;

    if (csts & 0x1) {
        printkf_info("NVMe controller already enabled\n");
        return 0;
    }

    uint64_t cap = ctrl->regs->cap;

    uint32_t cc = (1 << 0) | (0 << 4) | (0 << 7) | (0 << 11) | (6 << 16) | (4 << 20) | (0 << 24);

    ctrl->regs->cc = cc;

    uint8_t cap_to = (cap >> 24) & 0xFF;
    uint32_t timeout_ms = cap_to * 500;
    printkf_info("NVMe waiting for ready...\n", timeout_ms);

    uint32_t timeout = timeout_ms * 1000;
    uint32_t iterations = 0;

    while (!(ctrl->regs->csts & 0x1) && timeout--) {
        iterations++;
        for (volatile int i = 0; i < 1000; i++)
            ;
    }

    csts = ctrl->regs->csts;

    if (!(csts & 0x1)) {
        printkf_error("nvme_enable_controller(): Controller failed to become ready\n");
        printkf_error("nvme_enable_controller(): CSTS: 0x%x (RDY=%u, CFS=%u)\n", csts, csts & 0x1, (csts >> 1) & 0x1);
        return -1;
    }

    return 0;
}

static int nvme_identify_namespace(nvme_ctrl_t *ctrl, uint32_t nsid) {
    void *identify = pfallocator_request_page();
    memset(identify, 0, 4096);

    uint64_t identify_phys = virt_to_phys(identify);

    nvme_sqe_t cmd = {0};
    cmd.cdw0 = NVME_ADMIN_IDENTIFY;
    cmd.nsid = nsid;
    cmd.prp1 = identify_phys;
    cmd.cdw10 = 0;

    int ret = nvme_submit_command(&ctrl->admin_queue, &cmd, NULL);

    if (ret == 0) {
        uint64_t nsze = *(uint64_t *)identify;
        uint8_t flbas = *((uint8_t *)identify + 26);
        uint8_t lba_format_index = flbas & 0x0F;

        uint32_t *lbaf = (uint32_t *)((uint8_t *)identify + 128 + (lba_format_index * 4));
        uint8_t lbads = (*lbaf >> 16) & 0xFF;

        ctrl->block_size = 1 << lbads;
        ctrl->num_blocks = nsze;
    }

    pfallocator_free_page(identify);
    return ret;
}

static int nvme_create_io_queues(nvme_ctrl_t *ctrl, uint16_t num_queues) {
    ctrl->num_io_queues = num_queues;
    ctrl->io_queues = (nvme_queue_t *)malloc(num_queues * sizeof(nvme_queue_t));

    for (uint16_t i = 0; i < num_queues; i++) {
        uint16_t qid = i + 1;
        uint16_t queue_size = 64;

        nvme_queue_t *queue = &ctrl->io_queues[i];

        queue->sq = (nvme_sqe_t *)pfallocator_request_page();
        queue->cq = (nvme_cqe_t *)pfallocator_request_page();
        memset(queue->sq, 0, 4096);
        memset(queue->cq, 0, 4096);

        queue->size = queue_size;
        queue->sq_tail = 0;
        queue->cq_head = 0;
        queue->phase = 1;

        uint64_t sq_phys = virt_to_phys(queue->sq);
        uint64_t cq_phys = virt_to_phys(queue->cq);

        nvme_sqe_t cmd = {0};
        cmd.cdw0 = NVME_ADMIN_CREATE_IO_CQ;
        cmd.prp1 = cq_phys;
        cmd.cdw10 = ((queue_size - 1) << 16) | qid;
        cmd.cdw11 = 0x1;

        if (nvme_submit_command(&ctrl->admin_queue, &cmd, NULL) < 0) {
            printkf_error("nvme_create_io_queues(): Failed to create I/O CQ %u\n", qid);
            return -1;
        }

        memset(&cmd, 0, sizeof(cmd));
        cmd.cdw0 = NVME_ADMIN_CREATE_IO_SQ;
        cmd.prp1 = sq_phys;
        cmd.cdw10 = ((queue_size - 1) << 16) | qid;
        cmd.cdw11 = (qid << 16) | 0x1;

        if (nvme_submit_command(&ctrl->admin_queue, &cmd, NULL) < 0) {
            printkf_error("nvme_create_io_queues(): Failed to create I/O SQ %u\n", qid);
            return -1;
        }

        uint32_t sq_doorbell_offset = 0x1000 + (2 * qid * ctrl->doorbell_stride);
        uint32_t cq_doorbell_offset = 0x1000 + ((2 * qid + 1) * ctrl->doorbell_stride);

        queue->sq_doorbell = (uint32_t *)((uint8_t *)ctrl->regs + sq_doorbell_offset);
        queue->cq_doorbell = (uint32_t *)((uint8_t *)ctrl->regs + cq_doorbell_offset);
    }

    return 0;
}

int nvme_read(nvme_ctrl_t *ctrl, uint64_t lba, uint32_t num_blocks, void *buffer) {
    if (num_blocks == 0)
        return -1;

    uint64_t buffer_phys = virt_to_phys(buffer);

    nvme_sqe_t cmd = {0};
    cmd.cdw0 = NVME_CMD_READ;
    cmd.nsid = 1;
    cmd.prp1 = buffer_phys;

    cmd.cdw10 = lba & 0xFFFFFFFF;
    cmd.cdw11 = (lba >> 32) & 0xFFFFFFFF;
    cmd.cdw12 = (num_blocks - 1);

    nvme_queue_t *queue = ctrl->num_io_queues > 0 ? &ctrl->io_queues[0] : &ctrl->admin_queue;

    return nvme_submit_command(queue, &cmd, NULL);
}

int nvme_write(nvme_ctrl_t *ctrl, uint64_t lba, uint32_t num_blocks, const void *buffer) {
    if (num_blocks == 0)
        return -1;

    uint64_t buffer_phys = virt_to_phys((void *)buffer);

    nvme_sqe_t cmd = {0};
    cmd.cdw0 = NVME_CMD_WRITE;
    cmd.nsid = 1;
    cmd.prp1 = buffer_phys;

    cmd.cdw10 = lba & 0xFFFFFFFF;
    cmd.cdw11 = (lba >> 32) & 0xFFFFFFFF;
    cmd.cdw12 = (num_blocks - 1);

    nvme_queue_t *queue = ctrl->num_io_queues > 0 ? &ctrl->io_queues[0] : &ctrl->admin_queue;

    return nvme_submit_command(queue, &cmd, NULL);
}

static int nvme_probe(pci_device_t *pdev) {
    nvme_ctrl_t *ctrl = (nvme_ctrl_t *)malloc(sizeof(nvme_ctrl_t));
    if (!ctrl) {
        printkf_error("nvme_probe(): Failed to allocate NVMe controller\n");
        return -1;
    }
    memset(ctrl, 0, sizeof(nvme_ctrl_t));

    device_set_driver_data(&pdev->device, ctrl);

    ctrl->dma_buffer = pfallocator_request_page();
    if (!ctrl->dma_buffer) {
        printkf_error("nvme_probe(): Failed to allocate DMA buffer\n");
    }

    pci_enable_mmio(pdev);
    pci_enable_bus_mastering(pdev);

    ctrl->mmio_base = pci_map_bar(pdev, 0);
    if (!ctrl->mmio_base) {
        printkf_error("nvme_probe(): Failed to map NVMe BAR0\n");
        free(ctrl);
        return -1;
    }
    ctrl->regs = (volatile nvme_bar_t *)ctrl->mmio_base;

    uint64_t cap = ctrl->regs->cap;
    ctrl->max_queue_entries = ((cap >> 0) & 0xFFFF) + 1;
    ctrl->doorbell_stride = 4 << ((cap >> 32) & 0xF);

    if (nvme_reset_controller(ctrl) < 0) {
        printkf_error("nvme_probe(): Failed to reset NVMe controller\n");
        free(ctrl);
        return -1;
    }

    if (nvme_create_admin_queue(ctrl) < 0) {
        printkf_error("nvme_probe(): Failed to create admin queue\n");
        free(ctrl);
        return -1;
    }

    if (nvme_enable_controller(ctrl) < 0) {
        printkf_error("nvme_probe(): Failed to enable controller\n");
        free(ctrl);
        return -1;
    }

    if (nvme_identify_namespace(ctrl, 1) < 0) {
        printkf_error("nvme_probe(): Failed to identify namespace 1\n");
    }

    if (nvme_create_io_queues(ctrl, 1) < 0) {
        printkf_error("nvme_probe(): Failed to create IO queues\n");
    }

    nvme_register_device(ctrl);

    return 0;
}

static void nvme_remove(pci_device_t *pdev) {
    printkf_info("NVMe driver removing device %s\n", pdev->device.name);

    nvme_ctrl_t *ctrl = device_get_driver_data(&pdev->device);
    if (ctrl) {
        free(ctrl);
    }
}

static pci_driver_t nvme_driver = {
    .driver = {.name = "nvme"},
    .id_table = nvme_ids,
    .probe = nvme_probe,
    .remove = nvme_remove,
};

void nvme_driver_init() {
    printkf_info("Registering NVMe driver\n");
    pci_driver_register(&nvme_driver);
}