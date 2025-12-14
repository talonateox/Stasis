#include "partition.h"

#include "../../io/terminal.h"
#include "../../mem/alloc/heap.h"
#include "../../std/string.h"

typedef struct {
    uint8_t boot_flag;
    uint8_t chs_start[3];
    uint8_t type;
    uint8_t chs_end[3];
    uint32_t lba_start;
    uint32_t num_sectors;
} __attribute__((packed)) mbr_partition_entry_t;

typedef struct {
    uint8_t boot_code[446];
    mbr_partition_entry_t partitions[4];
    uint16_t signature;
} __attribute__((packed)) mbr_t;

typedef struct {
    char signature[8];
    uint32_t revision;
    uint32_t header_size;
    uint32_t header_crc32;
    uint32_t reserved;
    uint64_t current_lba;
    uint64_t backup_lba;
    uint64_t first_usable_lba;
    uint64_t last_usable_lba;
    uint8_t disk_guid[16];
    uint64_t partition_entry_lba;
    uint32_t num_partition_entries;
    uint32_t partition_entry_size;
    uint32_t partition_entries_crc32;
} __attribute__((packed)) gpt_header_t;

typedef struct {
    uint8_t type_guid[16];
    uint8_t partition_guid[16];
    uint64_t start_lba;
    uint64_t end_lba;
    uint64_t attributes;
    uint16_t name[36];
} __attribute__((packed)) gpt_entry_t;

static const uint8_t ESP_GUID[16] = {0x28, 0x73, 0x2A, 0xC1, 0x1F, 0xF8, 0xD2, 0x11,
                                     0xBA, 0x4B, 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B};

static const uint8_t BASIC_DATA_GUID[16] = {0xA2, 0xA0, 0xD0, 0xEB, 0xE5, 0xB9, 0x33, 0x44,
                                            0x87, 0xC0, 0x68, 0xB6, 0xB7, 0x26, 0x99, 0xC7};

typedef struct {
    char base_device[256];
    uint64_t offset;
    uint64_t size;
} partition_data_t;

static const char *partition_type_name(uint8_t type) {
    switch (type) {
    case 0x00:
        return "Empty";
    case 0x01:
        return "FAT12";
    case 0x04:
        return "FAT16 <32M";
    case 0x05:
        return "Extended";
    case 0x06:
        return "FAT16";
    case 0x07:
        return "NTFS/exFAT";
    case 0x0B:
        return "FAT32";
    case 0x0C:
        return "FAT32 LBA";
    case 0x0E:
        return "FAT16 LBA";
    case 0x0F:
        return "Extended LBA";
    case 0x82:
        return "Linux swap";
    case 0x83:
        return "Linux";
    case 0xEE:
        return "GPT";
    default:
        return "Unknown";
    }
}

static bool guid_is_zero(const uint8_t *guid) {
    for (int i = 0; i < 16; i++) {
        if (guid[i] != 0)
            return false;
    }
    return true;
}

static bool guid_equals(const uint8_t *a, const uint8_t *b) {
    return memcmp(a, b, 16) == 0;
}

static const char *gpt_type_name(const uint8_t *type_guid) {
    if (guid_equals(type_guid, ESP_GUID)) {
        return "EFI System";
    } else if (guid_equals(type_guid, BASIC_DATA_GUID)) {
        return "Basic Data";
    }
    return "Unknown";
}

static partition_table_t *partition_parse_gpt(int fd, const char *device_path) {
    gpt_header_t gpt;
    vfs_seek(fd, 512, SEEK_SET);
    if (vfs_read(fd, &gpt, sizeof(gpt_header_t)) != sizeof(gpt_header_t)) {
        printkf_error("partition_parse_gpt(): Failed to read GPT header\n");
        return NULL;
    }

    if (memcmp(gpt.signature, "EFI PART", 8) != 0) {
        printkf_error("partition_parse_gpt(): Invalid GPT signature\n");
        return NULL;
    }

    partition_table_t *table = (partition_table_t *)malloc(sizeof(partition_table_t));
    memset(table, 0, sizeof(partition_table_t));

    strncpy(table->device_path, device_path, sizeof(table->device_path) - 1);
    table->type = PARTITION_TYPE_GPT;
    table->num_partitions = 0;

    uint64_t entries_offset = gpt.partition_entry_lba * 512;
    uint32_t entries_to_read = gpt.num_partition_entries;
    if (entries_to_read > MAX_PARTITIONS) {
        entries_to_read = MAX_PARTITIONS;
    }

    gpt_entry_t *entry = (gpt_entry_t *)malloc(sizeof(gpt_entry_t));

    for (uint32_t i = 0; i < entries_to_read && table->num_partitions < MAX_PARTITIONS; i++) {
        vfs_seek(fd, entries_offset + (i * gpt.partition_entry_size), SEEK_SET);
        if (vfs_read(fd, entry, sizeof(gpt_entry_t)) != sizeof(gpt_entry_t)) {
            continue;
        }

        if (guid_is_zero(entry->type_guid)) {
            continue;
        }

        partition_info_t *part = &table->partitions[table->num_partitions];

        part->index = table->num_partitions + 1;
        part->type = 0;
        part->lba_start = entry->start_lba;
        part->num_sectors = entry->end_lba - entry->start_lba + 1;
        part->bootable = false;

        strncpy(part->type_name, gpt_type_name(entry->type_guid), sizeof(part->type_name) - 1);

        printkf_info("%sp%d: %s (GPT), LBA %llu, Size %llu mb\n", device_path, part->index, part->type_name,
                     part->lba_start, (part->num_sectors / 2048));

        table->num_partitions++;
    }

    free(entry);

    if (table->num_partitions == 0) {
        printkf_info("No GPT partitions found\n");
        free(table);
        return NULL;
    }

    return table;
}

partition_table_t *partition_parse_mbr(const char *device_path) {
    int fd = vfs_open(device_path, O_RDONLY);
    if (fd < 0) {
        printkf_error("partition_parse_mbr(): Failed to open device\n");
        return NULL;
    }

    mbr_t *mbr = (mbr_t *)malloc(sizeof(mbr_t));
    int64_t bytes = vfs_read(fd, mbr, 512);

    if (bytes != 512) {
        printkf_error("partition_parse_mbr(): Failed to read MBR (got %lld bytes)\n", bytes);
        free(mbr);
        vfs_close(fd);
        return NULL;
    }

    if (mbr->signature != 0xAA55) {
        printkf_error("partition_parse_mbr(): Invalid MBR signature: 0x%04x (expected 0xAA55)\n", mbr->signature);
        free(mbr);
        vfs_close(fd);
        return NULL;
    }

    for (int i = 0; i < 4; i++) {
        if (mbr->partitions[i].type == 0xEE) {
            free(mbr);
            partition_table_t *table = partition_parse_gpt(fd, device_path);
            vfs_close(fd);
            return table;
        }
    }

    vfs_close(fd);

    partition_table_t *table = (partition_table_t *)malloc(sizeof(partition_table_t));
    memset(table, 0, sizeof(partition_table_t));

    strncpy(table->device_path, device_path, sizeof(table->device_path) - 1);
    table->type = PARTITION_TYPE_MBR;
    table->num_partitions = 0;

    for (int i = 0; i < 4; i++) {
        mbr_partition_entry_t *entry = &mbr->partitions[i];

        if (entry->type == 0x00 || entry->num_sectors == 0) {
            continue;
        }

        partition_info_t *part = &table->partitions[table->num_partitions];

        part->index = i + 1;
        part->type = entry->type;
        part->lba_start = entry->lba_start;
        part->num_sectors = entry->num_sectors;
        part->bootable = (entry->boot_flag == 0x80);

        strncpy(part->type_name, partition_type_name(entry->type), sizeof(part->type_name) - 1);

        printkf_info("%sp%d: 0x%02x (%s), LBA %llu, Size %llu mb%s\n", device_path, part->index, part->type,
                     part->type_name, part->lba_start, (part->num_sectors / 2048), part->bootable ? " [BOOTABLE]" : "");

        table->num_partitions++;
    }

    free(mbr);

    if (table->num_partitions == 0) {
        printkf_info("No partitions found\n");
        free(table);
        return NULL;
    }

    return table;
}

void partition_free(partition_table_t *table) {
    if (table) {
        free(table);
    }
}

partition_info_t *partition_get(partition_table_t *table, int index) {
    if (!table || index < 1 || index > table->num_partitions) {
        return NULL;
    }

    for (int i = 0; i < table->num_partitions; i++) {
        if (table->partitions[i].index == index) {
            return &table->partitions[i];
        }
    }

    return NULL;
}

static int64_t partition_dev_read(vfs_node_t *node, void *buf, size_t size, size_t offset) {
    partition_data_t *pdata = (partition_data_t *)node->data;

    if (offset >= pdata->size) {
        return 0;
    }

    if (offset + size > pdata->size) {
        size = pdata->size - offset;
    }

    int fd = vfs_open(pdata->base_device, O_RDONLY);
    if (fd < 0)
        return -1;

    vfs_seek(fd, pdata->offset + offset, SEEK_SET);
    int64_t bytes = vfs_read(fd, buf, size);
    vfs_close(fd);

    return bytes;
}

static int64_t partition_dev_write(vfs_node_t *node, const void *buf, size_t size, size_t offset) {
    partition_data_t *pdata = (partition_data_t *)node->data;

    if (offset >= pdata->size) {
        return 0;
    }

    if (offset + size > pdata->size) {
        size = pdata->size - offset;
    }

    int fd = vfs_open(pdata->base_device, O_WRONLY);
    if (fd < 0)
        return -1;

    vfs_seek(fd, pdata->offset + offset, SEEK_SET);
    int64_t bytes = vfs_write(fd, buf, size);
    vfs_close(fd);

    return bytes;
}

static vfs_ops_t partition_dev_ops = {
    .read = partition_dev_read,
    .write = partition_dev_write,
    .create = NULL,
    .unlink = NULL,
    .truncate = NULL,
};

void partition_register(partition_table_t *table) {
    if (!table)
        return;

    for (int i = 0; i < table->num_partitions; i++) {
        partition_info_t *part = &table->partitions[i];

        char part_path[256];
        snprintf(part_path, sizeof(part_path), "%sp%d", table->device_path, part->index);

        vfs_node_t *part_node = vfs_create(part_path, VFS_FILE);
        if (!part_node) {
            printkf_error("partition_register(): Failed to create %s\n", part_path);
            continue;
        }

        partition_data_t *pdata = (partition_data_t *)malloc(sizeof(partition_data_t));
        strncpy(pdata->base_device, table->device_path, sizeof(pdata->base_device) - 1);
        pdata->offset = part->lba_start * 512;
        pdata->size = part->num_sectors * 512;

        part_node->ops = &partition_dev_ops;
        part_node->size = pdata->size;
        part_node->data = pdata;
    }
}
