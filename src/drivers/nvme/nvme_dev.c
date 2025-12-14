#include "../../fs/partition/partition.h"
#include "../../fs/vfs/vfs.h"
#include "../../io/terminal.h"
#include "../../mem/alloc/heap.h"
#include "../../mem/alloc/page_frame_alloc.h"
#include "../../std/string.h"
#include "nvme.h"

static nvme_device_node_t *device_list = NULL;
static int next_device_id = 0;

static nvme_ctrl_t *get_ctrl_from_node(vfs_node_t *node) {
    return (nvme_ctrl_t *)node->data;
}

static int64_t nvme_dev_read(vfs_node_t *node, void *buf, size_t size, size_t offset) {
    nvme_ctrl_t *ctrl = get_ctrl_from_node(node);
    if (!ctrl)
        return -1;

    uint64_t start_lba = offset / ctrl->block_size;
    size_t start_offset = offset % ctrl->block_size;

    uint64_t end_byte = offset + size;
    uint64_t end_lba = (end_byte + ctrl->block_size - 1) / ctrl->block_size;

    if (end_lba > ctrl->num_blocks) {
        printkf_error("nvme_dev_read(): Read beyond device bounds\n");
        return -1;
    }

    void *dma_buffer = ctrl->dma_buffer;
    size_t bytes_copied = 0;
    uint64_t current_lba = start_lba;

    while (current_lba < end_lba) {
        uint64_t blocks_remaining = end_lba - current_lba;
        uint16_t chunk = blocks_remaining > 8 ? 8 : (uint16_t)blocks_remaining;

        memset(dma_buffer, 0, 4096);

        if (nvme_read(ctrl, current_lba, chunk, dma_buffer) < 0) {
            printkf_error("nvme_dev_read(): NVMe read failed at block %llu\n", current_lba);
            return bytes_copied > 0 ? (int64_t)bytes_copied : -1;
        }

        size_t chunk_start = 0;
        size_t chunk_end = chunk * ctrl->block_size;

        if (current_lba == start_lba) {
            chunk_start = start_offset;
        }

        size_t bytes_from_start = (current_lba - start_lba) * ctrl->block_size;
        if (bytes_from_start + chunk_end > size + start_offset) {
            chunk_end = size + start_offset - bytes_from_start;
        }

        size_t copy_size = chunk_end - chunk_start;
        memcpy((uint8_t *)buf + bytes_copied, (uint8_t *)dma_buffer + chunk_start, copy_size);

        bytes_copied += copy_size;
        current_lba += chunk;
    }

    return bytes_copied;
}

static int64_t nvme_dev_write(vfs_node_t *node, const void *buf, size_t size, size_t offset) {
    nvme_ctrl_t *ctrl = get_ctrl_from_node(node);
    if (!ctrl)
        return -1;

    uint64_t start_lba = offset / ctrl->block_size;
    size_t start_offset = offset % ctrl->block_size;

    uint64_t end_byte = offset + size;
    uint64_t end_lba = (end_byte + ctrl->block_size - 1) / ctrl->block_size;

    if (end_lba > ctrl->num_blocks) {
        printkf_error("nvme_dev_write(): Write beyond device bounds\n");
        return -1;
    }

    void *dma_buffer = ctrl->dma_buffer;
    size_t bytes_written = 0;
    uint64_t current_lba = start_lba;

    while (current_lba < end_lba) {
        uint64_t blocks_remaining = end_lba - current_lba;
        uint16_t chunk = blocks_remaining > 8 ? 8 : (uint16_t)blocks_remaining;
        size_t chunk_bytes = chunk * ctrl->block_size;

        size_t write_start = 0;
        size_t write_end = chunk_bytes;

        if (current_lba == start_lba) {
            write_start = start_offset;
        }

        size_t bytes_from_start = (current_lba - start_lba) * ctrl->block_size;
        if (bytes_from_start + write_end > size + start_offset) {
            write_end = size + start_offset - bytes_from_start;
        }

        if (write_start != 0 || write_end != chunk_bytes) {
            if (nvme_read(ctrl, current_lba, chunk, dma_buffer) < 0) {
                printkf_error("nvme_dev_write(): Read-modify-write read failed\n");
                return bytes_written > 0 ? (int64_t)bytes_written : -1;
            }
        }

        size_t copy_size = write_end - write_start;
        memcpy((uint8_t *)dma_buffer + write_start, (uint8_t *)buf + bytes_written, copy_size);

        if (nvme_write(ctrl, current_lba, chunk, dma_buffer) < 0) {
            printkf_error("nvme_dev_write(): NVMe write failed at block %llu\n", current_lba);
            return bytes_written > 0 ? (int64_t)bytes_written : -1;
        }

        bytes_written += copy_size;
        current_lba += chunk;
    }

    return bytes_written;
}

static vfs_ops_t nvme_dev_ops = {
    .read = nvme_dev_read,
    .write = nvme_dev_write,
    .create = NULL,
    .unlink = NULL,
    .truncate = NULL,
};

void nvme_register_device(nvme_ctrl_t *ctrl) {
    nvme_device_node_t *dev_node = (nvme_device_node_t *)malloc(sizeof(nvme_device_node_t));
    dev_node->ctrl = ctrl;
    dev_node->device_id = next_device_id++;
    dev_node->next = device_list;
    device_list = dev_node;

    vfs_node_t *dev = vfs_lookup("/dev");
    if (!dev) {
        vfs_mkdir("/dev");
    }

    char dev_path[64];
    snprintf(dev_path, sizeof(dev_path), "/dev/nvme%d", dev_node->device_id);

    vfs_node_t *nvme_node = vfs_create(dev_path, VFS_FILE);
    if (!nvme_node) {
        printkf_error("nvme_register_device(): Failed to create %s\n", dev_path);
        return;
    }

    nvme_node->ops = &nvme_dev_ops;
    nvme_node->size = ctrl->num_blocks * ctrl->block_size;
    nvme_node->data = ctrl;

    printkf_ok("Registered NVMe device at %s (%llu MB)\n", dev_path, nvme_node->size / (1024 * 1024));

    partition_table_t *table = partition_parse_mbr(dev_path);
    if (table) {
        partition_register(table);
        partition_free(table);
    } else {
        printkf_info("No valid partition table found on %s\n", dev_path);
    }
}

nvme_device_node_t *nvme_get_devices(void) {
    return device_list;
}