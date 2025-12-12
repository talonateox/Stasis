#include "mount.h"

#include "../../io/terminal.h"
#include "../../mem/alloc/heap.h"
#include "../../std/string.h"
#include "../fat32/fat32.h"
#include "../vfs/vfs.h"

static mount_point_t* mount_list_head = NULL;

void mount_init(void) {
    mount_list_head = NULL;
    printkf_ok("Mount manager initialized\n");
}

mount_point_t* mount_get_for_path(const char* path) {
    mount_point_t* best_match = NULL;
    size_t best_match_len = 0;

    mount_point_t* current = mount_list_head;
    while (current) {
        size_t mp_len = strlen(current->mountpoint);

        if (strncmp(path, current->mountpoint, mp_len) == 0) {
            if (path[mp_len] == '\0' || path[mp_len] == '/') {
                if (mp_len > best_match_len) {
                    best_match = current;
                    best_match_len = mp_len;
                }
            }
        }

        current = current->next;
    }

    return best_match;
}

int mount(const char* device, const char* mountpoint, const char* fs_type) {
    mount_point_t* existing = mount_list_head;
    while (existing) {
        if (strcmp(existing->mountpoint, mountpoint) == 0) {
            printkf_error("mount(): %s already in use\n", mountpoint);
            return -1;
        }
        existing = existing->next;
    }

    mount_point_t* mp = (mount_point_t*)malloc(sizeof(mount_point_t));
    if (!mp) {
        printkf_error("mount(): Failed to allocate mount point\n");
        return -1;
    }

    memset(mp, 0, sizeof(mount_point_t));
    strncpy(mp->device, device, sizeof(mp->device) - 1);
    strncpy(mp->mountpoint, mountpoint, sizeof(mp->mountpoint) - 1);

    if (strcmp(fs_type, "fat32") == 0) {
        mp->fs_type = FS_TYPE_FAT32;

        if (fat32_mount_vfs(device, mountpoint, (void**)&mp->fs_data) < 0) {
            printkf_error("mount(): Failed to mount FAT32\n");
            free(mp);
            return -1;
        }
    } else if (strcmp(fs_type, "tmpfs") == 0) {
        mp->fs_type = FS_TYPE_TMPFS;
        mp->fs_data = NULL;
    } else {
        printkf_error("mount(): Unsupported filesystem type: %s\n", fs_type);
        free(mp);
        return -1;
    }

    mp->next = mount_list_head;
    mount_list_head = mp;

    return 0;
}

int unmount(const char* mountpoint) {
    mount_point_t** current = &mount_list_head;

    while (*current) {
        if (strcmp((*current)->mountpoint, mountpoint) == 0) {
            mount_point_t* mp = *current;

            if (mp->fs_type == FS_TYPE_FAT32 && mp->fs_data) {
                fat32_unmount_vfs(mp->fs_data, mountpoint);
            }

            *current = mp->next;
            free(mp);
            return 0;
        }
        current = &(*current)->next;
    }

    printkf_error("unmount(): No filesystem mounted at %s\n", mountpoint);
    return -1;
}

void mount_list(void) {
    printkf_info("Mounted filesystems:\n");

    mount_point_t* current = mount_list_head;
    int count = 0;

    while (current) {
        const char* type_name = "unknown";
        switch (current->fs_type) {
            case FS_TYPE_TMPFS:
                type_name = "tmpfs";
                break;
            case FS_TYPE_FAT32:
                type_name = "fat32";
                break;
            default:
                break;
        }

        printkf_info("  %s on %s type %s\n", current->device,
                     current->mountpoint, type_name);

        current = current->next;
        count++;
    }

    if (count == 0) {
        printkf_info("  (none)\n");
    }
}