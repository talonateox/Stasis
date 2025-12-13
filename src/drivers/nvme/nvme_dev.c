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

    uint64_t lba = offset / ctrl->block_size;
    size_t byte_offset = offset % ctrl->block_size;

    if (byte_offset != 0) {
        printkf_error("nvme_dev_read(): Unaligned read not supported yet\n");
        return -1;
    }

    if (size % ctrl->block_size != 0) {
        printkf_error("nvme_dev_read(): Size must be multiple of block size\n");
        return -1;
    }

    uint16_t num_blocks = size / ctrl->block_size;

    if (lba + num_blocks > ctrl->num_blocks) {
        printkf_error("nvme_dev_read(): Read beyond device bounds\n");
        return -1;
    }

    void *dma_buffer = ctrl->dma_buffer;

    size_t bytes_read = 0;
    size_t blocks_read = 0;

    while (blocks_read < num_blocks) {
        uint16_t chunk = (num_blocks - blocks_read) > 8 ? 8 : (num_blocks - blocks_read);

        memset(dma_buffer, 0, 4096);

        if (nvme_read(ctrl, lba + blocks_read, chunk, dma_buffer) < 0) {
            printkf_error("nvme_dev_read(): NVMe read failed at block %llu\n", lba + blocks_read);
            return bytes_read;
        }

        size_t chunk_bytes = chunk * ctrl->block_size;
        memcpy((uint8_t *)buf + bytes_read, dma_buffer, chunk_bytes);

        bytes_read += chunk_bytes;
        blocks_read += chunk;
    }

    return bytes_read;
}

static int64_t nvme_dev_write(vfs_node_t *node, const void *buf, size_t size, size_t offset) {
    nvme_ctrl_t *ctrl = get_ctrl_from_node(node);
    if (!ctrl)
        return -1;

    uint64_t lba = offset / ctrl->block_size;
    size_t byte_offset = offset % ctrl->block_size;

    if (byte_offset != 0) {
        printkf_error("nvme_dev_write(): Unaligned write not supported yet\n");
        return -1;
    }

    if (size % ctrl->block_size != 0) {
        printkf_error("nvme_dev_write(): Size must be multiple of block size\n");
        return -1;
    }

    uint16_t num_blocks = size / ctrl->block_size;

    if (lba + num_blocks > ctrl->num_blocks) {
        printkf_error("nvme_dev_write(): Write beyond device bounds\n");
        return -1;
    }

    void *dma_buffer = ctrl->dma_buffer;

    size_t bytes_written = 0;
    size_t blocks_written = 0;

    while (blocks_written < num_blocks) {
        uint16_t chunk = (num_blocks - blocks_written) > 8 ? 8 : (num_blocks - blocks_written);

        size_t chunk_bytes = chunk * ctrl->block_size;
        memcpy(dma_buffer, (uint8_t *)buf + bytes_written, chunk_bytes);

        if (nvme_write(ctrl, lba + blocks_written, chunk, dma_buffer) < 0) {
            printkf_error("nvme_dev_write(): NVMe write failed at block %llu\n", lba + blocks_written);
            return bytes_written;
        }

        bytes_written += chunk_bytes;
        blocks_written += chunk;
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