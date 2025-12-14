#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../vfs/vfs.h"

#define FAT32_ATTR_READ_ONLY 0x01
#define FAT32_ATTR_HIDDEN 0x02
#define FAT32_ATTR_SYSTEM 0x04
#define FAT32_ATTR_VOLUME_ID 0x08
#define FAT32_ATTR_DIRECTORY 0x10
#define FAT32_ATTR_ARCHIVE 0x20
#define FAT32_ATTR_LONG_NAME 0x0F

#define FAT32_EOC 0x0FFFFFF8
#define FAT32_BAD_CLUSTER 0x0FFFFFF7
#define FAT32_FREE_CLUSTER 0x00000000

typedef struct {
    uint8_t jump[3];
    char oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t num_fats;
    uint16_t root_entries;
    uint16_t total_sectors_16;
    uint8_t media_descriptor;
    uint16_t sectors_per_fat_16;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;

    uint32_t sectors_per_fat_32;
    uint16_t flags;
    uint16_t version;
    uint32_t root_cluster;
    uint16_t fsinfo_sector;
    uint16_t backup_boot_sector;
    uint8_t reserved[12];
    uint8_t drive_number;
    uint8_t reserved1;
    uint8_t boot_signature;
    uint32_t volume_id;
    char volume_label[11];
    char fs_type[8];
    uint8_t boot_code[420];
    uint16_t signature;
} __attribute__((packed)) fat32_boot_sector_t;

typedef struct {
    char name[11];
    uint8_t attributes;
    uint8_t reserved;
    uint8_t creation_time_tenth;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t last_access_date;
    uint16_t cluster_high;
    uint16_t last_mod_time;
    uint16_t last_mod_date;
    uint16_t cluster_low;
    uint32_t file_size;
} __attribute__((packed)) fat32_dir_entry_t;

typedef struct {
    uint8_t order;
    uint16_t name1[5];
    uint8_t attributes;
    uint8_t type;
    uint8_t checksum;
    uint16_t name2[6];
    uint16_t cluster;
    uint16_t name3[2];
} __attribute__((packed)) fat32_lfn_entry_t;

typedef struct {
    char device_path[256];
    int device_fd;

    fat32_boot_sector_t boot;

    uint32_t fat_start_sector;
    uint32_t data_start_sector;
    uint32_t total_clusters;
    uint32_t bytes_per_cluster;

    uint32_t *fat_cache;
    bool fat_cache_dirty;
} fat32_fs_t;

fat32_fs_t *fat32_mount(const char *device_path);
void fat32_unmount(fat32_fs_t *fs);
int fat32_mount_vfs(const char *device_path, const char *mountpoint, void **fs_data_out);
void fat32_unmount_vfs(void *fs_data, const char *mountpoint);

int fat32_read_file(fat32_fs_t *fs, fat32_dir_entry_t *entry, void *buffer, size_t size);
