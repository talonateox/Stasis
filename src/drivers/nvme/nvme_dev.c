#include "../../fs/vfs/vfs.h"
#include "../../io/terminal.h"
#include "../../mem/alloc/heap.h"
#include "../../std/string.h"
#include "nvme.h"

static nvme_ctrl_t* global_nvme_ctrl = NULL;

static int64_t nvme_dev_read(vfs_node_t* node, void* buf, size_t size,
                             size_t offset) {
    if (!global_nvme_ctrl) return -1;

    uint64_t lba = offset / global_nvme_ctrl->block_size;
    size_t byte_offset = offset % global_nvme_ctrl->block_size;

    if (byte_offset != 0) {
        printkf_error("nvme_dev_read(): Unaligned read not supported yet\n");
        return -1;
    }

    if (size % global_nvme_ctrl->block_size != 0) {
        printkf_error("nvme_dev_read(): Size must be multiple of block size\n");
        return -1;
    }

    uint16_t num_blocks = size / global_nvme_ctrl->block_size;

    if (lba + num_blocks > global_nvme_ctrl->num_blocks) {
        printkf_error("nvme_dev_read(): Read beyond device bounds\n");
        return -1;
    }

    if (nvme_read(global_nvme_ctrl, lba, num_blocks, buf) < 0) {
        return -1;
    }

    return size;
}

static int64_t nvme_dev_write(vfs_node_t* node, const void* buf, size_t size,
                              size_t offset) {
    if (!global_nvme_ctrl) return -1;

    uint64_t lba = offset / global_nvme_ctrl->block_size;
    size_t byte_offset = offset % global_nvme_ctrl->block_size;

    if (byte_offset != 0) {
        printkf_error("nvme_dev_write(): Unaligned write not supported yet\n");
        return -1;
    }

    if (size % global_nvme_ctrl->block_size != 0) {
        printkf_error(
            "nvme_dev_write(): Size must be multiple of block size\n");
        return -1;
    }

    uint16_t num_blocks = size / global_nvme_ctrl->block_size;

    if (lba + num_blocks > global_nvme_ctrl->num_blocks) {
        printkf_error("nvme_dev_write(): Write beyond device bounds\n");
        return -1;
    }

    if (nvme_write(global_nvme_ctrl, lba, num_blocks, buf) < 0) {
        return -1;
    }

    return size;
}

static vfs_ops_t nvme_dev_ops = {
    .read = nvme_dev_read,
    .write = nvme_dev_write,
    .create = NULL,
    .unlink = NULL,
    .truncate = NULL,
};

void nvme_register_device(nvme_ctrl_t* ctrl) {
    global_nvme_ctrl = ctrl;

    vfs_node_t* dev = vfs_lookup("/dev");
    if (!dev) {
        vfs_mkdir("/dev");
        dev = vfs_lookup("/dev");
    }

    vfs_node_t* nvme_node = vfs_create("/dev/nvme0", VFS_FILE);
    if (!nvme_node) {
        printkf_error("Failed to create /dev/nvme0\n");
        return;
    }

    nvme_node->ops = &nvme_dev_ops;
    nvme_node->size = ctrl->num_blocks * ctrl->block_size;

    printkf_ok("Registered NVMe device at /dev/nvme0 (%llu MB)\n",
               nvme_node->size / (1024 * 1024));
}