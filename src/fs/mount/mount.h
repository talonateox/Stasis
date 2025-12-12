#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct mount_point mount_point_t;

typedef enum {
    FS_TYPE_TMPFS,
    FS_TYPE_FAT32,
} fs_type_t;

struct mount_point {
    char device[256];
    char mountpoint[256];
    fs_type_t fs_type;
    void* fs_data;
    mount_point_t* next;
};

void mount_init(void);
int mount(const char* device, const char* mountpoint, const char* fs_type);
int unmount(const char* mountpoint);
mount_point_t* mount_get_for_path(const char* path);
void mount_list(void);