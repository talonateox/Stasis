#include "fat32.h"

#include "../../io/terminal.h"
#include "../../mem/alloc/heap.h"
#include "../../std/string.h"

static uint32_t cluster_to_sector(fat32_fs_t* fs, uint32_t cluster) {
    return fs->data_start_sector +
           ((cluster - 2) * fs->boot.sectors_per_cluster);
}

static uint32_t fat32_get_next_cluster(fat32_fs_t* fs, uint32_t cluster) {
    if (cluster < 2 || cluster >= fs->total_clusters + 2) {
        return FAT32_EOC;
    }

    uint32_t next = fs->fat_cache[cluster] & 0x0FFFFFFF;

    if (next >= FAT32_EOC) {
        return FAT32_EOC;
    }

    return next;
}

static void fat32_set_fat_entry(fat32_fs_t* fs, uint32_t cluster,
                                uint32_t value) {
    if (cluster < 2 || cluster >= fs->total_clusters + 2) {
        return;
    }

    fs->fat_cache[cluster] = value & 0x0FFFFFFF;
    fs->fat_cache_dirty = true;
}

static uint32_t fat32_find_free_cluster(fat32_fs_t* fs) {
    for (uint32_t i = 2; i < fs->total_clusters + 2; i++) {
        if ((fs->fat_cache[i] & 0x0FFFFFFF) == FAT32_FREE_CLUSTER) {
            return i;
        }
    }
    return 0;
}

static uint32_t fat32_allocate_cluster(fat32_fs_t* fs,
                                       uint32_t previous_cluster) {
    uint32_t new_cluster = fat32_find_free_cluster(fs);
    if (new_cluster == 0) {
        printkf_error("fat32_allocate_cluster(): No free clusters available\n");
        return 0;
    }

    fat32_set_fat_entry(fs, new_cluster, FAT32_EOC);

    if (previous_cluster >= 2) {
        fat32_set_fat_entry(fs, previous_cluster, new_cluster);
    }

    return new_cluster;
}

static void fat32_format_filename(const char* fat_name, char* output) {
    int out_idx = 0;

    for (int i = 0; i < 8 && fat_name[i] != ' '; i++) {
        output[out_idx++] = fat_name[i];
    }

    if (fat_name[8] != ' ') {
        output[out_idx++] = '.';
        for (int i = 8; i < 11 && fat_name[i] != ' '; i++) {
            output[out_idx++] = fat_name[i];
        }
    }

    output[out_idx] = '\0';
}

static void fat32_create_83_name(const char* filename, char* fat_name) {
    memset(fat_name, ' ', 11);

    int name_len = 0;
    int ext_len = 0;
    const char* ext = NULL;

    for (int i = 0; filename[i]; i++) {
        if (filename[i] == '.') {
            ext = &filename[i + 1];
            name_len = i;
            break;
        }
    }

    if (!ext) {
        name_len = strlen(filename);
    } else {
        ext_len = strlen(ext);
    }

    if (name_len > 8) name_len = 8;
    for (int i = 0; i < name_len; i++) {
        char c = filename[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        fat_name[i] = c;
    }

    if (ext) {
        if (ext_len > 3) ext_len = 3;
        for (int i = 0; i < ext_len; i++) {
            char c = ext[i];
            if (c >= 'a' && c <= 'z') c -= 32;
            fat_name[8 + i] = c;
        }
    }
}

static int fat32_read_cluster(fat32_fs_t* fs, uint32_t cluster, void* buffer) {
    if (cluster < 2 || cluster >= fs->total_clusters + 2) {
        printkf_error("fat32_read_cluster(): Invalid cluster: %u\n", cluster);
        return -1;
    }

    uint32_t sector = cluster_to_sector(fs, cluster);
    uint64_t offset = sector * fs->boot.bytes_per_sector;

    vfs_seek(fs->device_fd, offset, SEEK_SET);
    int64_t bytes = vfs_read(fs->device_fd, buffer, fs->bytes_per_cluster);

    if (bytes != (int64_t)fs->bytes_per_cluster) {
        printkf_error("fat32_read_cluster(): Failed to read cluster %u\n",
                      cluster);
        return -1;
    }

    return 0;
}

static int fat32_write_cluster(fat32_fs_t* fs, uint32_t cluster,
                               const void* buffer) {
    if (cluster < 2 || cluster >= fs->total_clusters + 2) {
        printkf_error("fat32_write_cluster(): Invalid cluster: %u\n", cluster);
        return -1;
    }

    uint32_t sector = cluster_to_sector(fs, cluster);
    uint64_t offset = sector * fs->boot.bytes_per_sector;

    vfs_seek(fs->device_fd, offset, SEEK_SET);
    int64_t bytes = vfs_write(fs->device_fd, buffer, fs->bytes_per_cluster);

    if (bytes != (int64_t)fs->bytes_per_cluster) {
        printkf_error("fat32_write_cluster(): Failed to write cluster %u\n",
                      cluster);
        return -1;
    }

    return 0;
}

static int fat32_flush_fat(fat32_fs_t* fs) {
    if (!fs->fat_cache_dirty) {
        return 0;
    }

    printkf_info("Flushing FAT to disk\n");

    size_t fat_size_bytes =
        fs->boot.sectors_per_fat_32 * fs->boot.bytes_per_sector;

    for (int i = 0; i < fs->boot.num_fats; i++) {
        uint32_t fat_sector =
            fs->fat_start_sector + (i * fs->boot.sectors_per_fat_32);
        uint64_t offset = fat_sector * fs->boot.bytes_per_sector;

        vfs_seek(fs->device_fd, offset, SEEK_SET);
        int64_t bytes = vfs_write(fs->device_fd, fs->fat_cache, fat_size_bytes);

        if (bytes != (int64_t)fat_size_bytes) {
            printkf_error("fat32_flush_fat(): Failed to write FAT %d\n", i);
            return -1;
        }
    }

    fs->fat_cache_dirty = false;
    printkf_ok("FAT flushed successfully\n");

    return 0;
}

static int fat32_read_directory(fat32_fs_t* fs, uint32_t cluster,
                                fat32_dir_entry_t** entries, int* count) {
    *entries = NULL;
    *count = 0;

    uint8_t* buffer = (uint8_t*)malloc(fs->bytes_per_cluster);
    if (!buffer) {
        printkf_error(
            "fat32_read_directory(): Failed to allocate cluster buffer\n");
        return -1;
    }

    int total_entries = 0;
    uint32_t current_cluster = cluster;

    while (current_cluster < FAT32_EOC) {
        total_entries += fs->bytes_per_cluster / sizeof(fat32_dir_entry_t);
        current_cluster = fat32_get_next_cluster(fs, current_cluster);
    }

    fat32_dir_entry_t* entry_array =
        (fat32_dir_entry_t*)malloc(total_entries * sizeof(fat32_dir_entry_t));
    if (!entry_array) {
        free(buffer);
        return -1;
    }

    int entry_idx = 0;
    current_cluster = cluster;

    while (current_cluster < FAT32_EOC) {
        if (fat32_read_cluster(fs, current_cluster, buffer) < 0) {
            free(buffer);
            free(entry_array);
            return -1;
        }

        fat32_dir_entry_t* entries_in_cluster = (fat32_dir_entry_t*)buffer;
        int entries_per_cluster =
            fs->bytes_per_cluster / sizeof(fat32_dir_entry_t);

        for (int i = 0; i < entries_per_cluster; i++) {
            fat32_dir_entry_t* entry = &entries_in_cluster[i];

            if (entry->name[0] == 0x00) {
                goto done;
            }

            if ((uint8_t)entry->name[0] == 0xE5) {
                continue;
            }

            if (entry->attributes == FAT32_ATTR_LONG_NAME) {
                continue;
            }

            if (entry->attributes & FAT32_ATTR_VOLUME_ID) {
                continue;
            }

            memcpy(&entry_array[entry_idx], entry, sizeof(fat32_dir_entry_t));
            entry_idx++;
        }

        current_cluster = fat32_get_next_cluster(fs, current_cluster);
    }

done:
    free(buffer);

    *entries = entry_array;
    *count = entry_idx;

    return 0;
}

static fat32_dir_entry_t* fat32_find_file(fat32_fs_t* fs, uint32_t dir_cluster,
                                          const char* filename) {
    fat32_dir_entry_t* entries;
    int count;

    if (fat32_read_directory(fs, dir_cluster, &entries, &count) < 0) {
        return NULL;
    }

    for (int i = 0; i < count; i++) {
        char entry_name[13];
        fat32_format_filename(entries[i].name, entry_name);

        bool match = true;
        for (int j = 0; entry_name[j] && filename[j]; j++) {
            char c1 = entry_name[j];
            char c2 = filename[j];

            if (c1 >= 'a' && c1 <= 'z') c1 -= 32;
            if (c2 >= 'a' && c2 <= 'z') c2 -= 32;

            if (c1 != c2) {
                match = false;
                break;
            }
        }

        if (match && entry_name[strlen(filename)] == '\0') {
            fat32_dir_entry_t* result =
                (fat32_dir_entry_t*)malloc(sizeof(fat32_dir_entry_t));
            memcpy(result, &entries[i], sizeof(fat32_dir_entry_t));
            free(entries);
            return result;
        }
    }

    free(entries);
    return NULL;
}

static int fat32_add_dir_entry(fat32_fs_t* fs, uint32_t dir_cluster,
                               fat32_dir_entry_t* new_entry) {
    uint8_t* buffer = (uint8_t*)malloc(fs->bytes_per_cluster);
    if (!buffer) {
        return -1;
    }

    uint32_t current_cluster = dir_cluster;
    uint32_t last_cluster = dir_cluster;

    while (current_cluster < FAT32_EOC) {
        last_cluster = current_cluster;

        if (fat32_read_cluster(fs, current_cluster, buffer) < 0) {
            free(buffer);
            return -1;
        }

        fat32_dir_entry_t* entries = (fat32_dir_entry_t*)buffer;
        int entries_per_cluster =
            fs->bytes_per_cluster / sizeof(fat32_dir_entry_t);

        for (int i = 0; i < entries_per_cluster; i++) {
            if (entries[i].name[0] == 0x00 ||
                (uint8_t)entries[i].name[0] == 0xE5) {
                memcpy(&entries[i], new_entry, sizeof(fat32_dir_entry_t));

                if (fat32_write_cluster(fs, current_cluster, buffer) < 0) {
                    free(buffer);
                    return -1;
                }

                free(buffer);
                return 0;
            }
        }

        current_cluster = fat32_get_next_cluster(fs, current_cluster);
    }

    uint32_t new_cluster = fat32_allocate_cluster(fs, last_cluster);
    if (new_cluster == 0) {
        free(buffer);
        return -1;
    }

    memset(buffer, 0, fs->bytes_per_cluster);

    fat32_dir_entry_t* entries = (fat32_dir_entry_t*)buffer;
    memcpy(&entries[0], new_entry, sizeof(fat32_dir_entry_t));

    if (fat32_write_cluster(fs, new_cluster, buffer) < 0) {
        free(buffer);
        return -1;
    }

    free(buffer);
    return 0;
}

int fat32_read_file(fat32_fs_t* fs, fat32_dir_entry_t* entry, void* buffer,
                    size_t size) {
    if (!fs || !entry || !buffer) return -1;

    uint32_t cluster =
        ((uint32_t)entry->cluster_high << 16) | entry->cluster_low;

    if (cluster < 2) {
        printkf_error("fat32_read_file(): Invalid cluster\n");
        return -1;
    }

    if (size > entry->file_size) {
        size = entry->file_size;
    }

    uint8_t* cluster_buffer = (uint8_t*)malloc(fs->bytes_per_cluster);
    if (!cluster_buffer) {
        printkf_error("fat32_read_file(): Failed to allocate cluster buffer\n");
        return -1;
    }

    size_t bytes_read = 0;

    while (cluster < FAT32_EOC && bytes_read < size) {
        if (fat32_read_cluster(fs, cluster, cluster_buffer) < 0) {
            free(cluster_buffer);
            return -1;
        }

        size_t to_copy = fs->bytes_per_cluster;
        if (bytes_read + to_copy > size) {
            to_copy = size - bytes_read;
        }

        memcpy((uint8_t*)buffer + bytes_read, cluster_buffer, to_copy);
        bytes_read += to_copy;

        cluster = fat32_get_next_cluster(fs, cluster);
    }

    free(cluster_buffer);

    return bytes_read;
}

int fat32_write_file(fat32_fs_t* fs, fat32_dir_entry_t* entry,
                     const void* buffer, size_t size, bool append) {
    if (!fs || !entry || !buffer) return -1;

    uint32_t cluster =
        ((uint32_t)entry->cluster_high << 16) | entry->cluster_low;

    if (cluster < 2) {
        cluster = fat32_allocate_cluster(fs, 0);
        if (cluster == 0) return -1;

        entry->cluster_high = (cluster >> 16) & 0xFFFF;
        entry->cluster_low = cluster & 0xFFFF;
    }

    uint8_t* cluster_buffer = (uint8_t*)malloc(fs->bytes_per_cluster);
    if (!cluster_buffer) {
        return -1;
    }

    size_t bytes_written = 0;
    uint32_t current_cluster = cluster;

    if (append) {
        while (fat32_get_next_cluster(fs, current_cluster) < FAT32_EOC) {
            current_cluster = fat32_get_next_cluster(fs, current_cluster);
        }
    }

    while (bytes_written < size) {
        size_t to_write = fs->bytes_per_cluster;
        if (bytes_written + to_write > size) {
            to_write = size - bytes_written;

            memset(cluster_buffer, 0, fs->bytes_per_cluster);
        }

        memcpy(cluster_buffer, (uint8_t*)buffer + bytes_written, to_write);

        if (fat32_write_cluster(fs, current_cluster, cluster_buffer) < 0) {
            free(cluster_buffer);
            return -1;
        }

        bytes_written += to_write;

        if (bytes_written < size) {
            uint32_t next_cluster = fat32_get_next_cluster(fs, current_cluster);

            if (next_cluster >= FAT32_EOC) {
                next_cluster = fat32_allocate_cluster(fs, current_cluster);
                if (next_cluster == 0) {
                    free(cluster_buffer);
                    return -1;
                }
            }

            current_cluster = next_cluster;
        }
    }

    free(cluster_buffer);

    if (append) {
        entry->file_size += bytes_written;
    } else {
        entry->file_size = bytes_written;
    }

    return bytes_written;
}

int fat32_create_file(fat32_fs_t* fs, uint32_t dir_cluster,
                      const char* filename) {
    fat32_dir_entry_t* existing = fat32_find_file(fs, dir_cluster, filename);
    if (existing) {
        free(existing);
        printkf_error("fat32_create_file(): File already exists\n");
        return -1;
    }

    fat32_dir_entry_t new_entry;
    memset(&new_entry, 0, sizeof(fat32_dir_entry_t));

    fat32_create_83_name(filename, new_entry.name);

    new_entry.attributes = FAT32_ATTR_ARCHIVE;
    new_entry.cluster_high = 0;
    new_entry.cluster_low = 0;
    new_entry.file_size = 0;

    if (fat32_add_dir_entry(fs, dir_cluster, &new_entry) < 0) {
        printkf_error("fat32_create_file(): Failed to add directory entry\n");
        return -1;
    }

    if (fat32_flush_fat(fs) < 0) {
        printkf_error("fat32_create_file(): Failed to flush FAT\n");
        return -1;
    }

    return 0;
}

int fat32_create_directory(fat32_fs_t* fs, uint32_t parent_cluster,
                           const char* dirname) {
    fat32_dir_entry_t* existing = fat32_find_file(fs, parent_cluster, dirname);
    if (existing) {
        free(existing);
        printkf_error("fat32_create_directory(): Directory already exists\n");
        return -1;
    }

    uint32_t new_cluster = fat32_allocate_cluster(fs, 0);
    if (new_cluster == 0) {
        return -1;
    }

    uint8_t* cluster_buffer = (uint8_t*)malloc(fs->bytes_per_cluster);
    if (!cluster_buffer) {
        return -1;
    }
    memset(cluster_buffer, 0, fs->bytes_per_cluster);

    fat32_dir_entry_t* entries = (fat32_dir_entry_t*)cluster_buffer;

    memset(&entries[0].name, ' ', 11);
    entries[0].name[0] = '.';
    entries[0].attributes = FAT32_ATTR_DIRECTORY;
    entries[0].cluster_high = (new_cluster >> 16) & 0xFFFF;
    entries[0].cluster_low = new_cluster & 0xFFFF;

    memset(&entries[1].name, ' ', 11);
    entries[1].name[0] = '.';
    entries[1].name[1] = '.';
    entries[1].attributes = FAT32_ATTR_DIRECTORY;
    entries[1].cluster_high = (parent_cluster >> 16) & 0xFFFF;
    entries[1].cluster_low = parent_cluster & 0xFFFF;

    if (fat32_write_cluster(fs, new_cluster, cluster_buffer) < 0) {
        free(cluster_buffer);
        return -1;
    }

    free(cluster_buffer);

    fat32_dir_entry_t new_entry;
    memset(&new_entry, 0, sizeof(fat32_dir_entry_t));

    fat32_create_83_name(dirname, new_entry.name);

    new_entry.attributes = FAT32_ATTR_DIRECTORY;
    new_entry.cluster_high = (new_cluster >> 16) & 0xFFFF;
    new_entry.cluster_low = new_cluster & 0xFFFF;
    new_entry.file_size = 0;

    if (fat32_add_dir_entry(fs, parent_cluster, &new_entry) < 0) {
        printkf_error(
            "fat32_create_directory(): Failed to add directory entry\n");
        return -1;
    }

    if (fat32_flush_fat(fs) < 0) {
        printkf_error("fat32_create_directory(): Failed to flush FAT\n");
        return -1;
    }

    return 0;
}

fat32_fs_t* fat32_mount(const char* device_path) {
    printkf_info("Mounting FAT32 from %s\n", device_path);

    int fd = vfs_open(device_path, O_RDWR);
    if (fd < 0) {
        printkf_error("fat32_mount(): Failed to open device %s\n", device_path);
        return NULL;
    }

    fat32_fs_t* fs = (fat32_fs_t*)malloc(sizeof(fat32_fs_t));
    memset(fs, 0, sizeof(fat32_fs_t));

    strncpy(fs->device_path, device_path, sizeof(fs->device_path) - 1);
    fs->device_fd = fd;

    vfs_seek(fd, 0, SEEK_SET);
    if (vfs_read(fd, &fs->boot, sizeof(fat32_boot_sector_t)) !=
        sizeof(fat32_boot_sector_t)) {
        printkf_error("fat32_mount(): Failed to read boot sector\n");
        vfs_close(fd);
        free(fs);
        return NULL;
    }

    if (fs->boot.signature != 0xAA55) {
        printkf_error("fat32_mount(): Invalid boot signature: 0x%04x\n",
                      fs->boot.signature);
        vfs_close(fd);
        free(fs);
        return NULL;
    }

    if (fs->boot.sectors_per_fat_16 != 0 || fs->boot.root_entries != 0) {
        printkf_error(
            "fat32_mount(): Not a FAT32 filesystem (FAT12/16 detected)\n");
        vfs_close(fd);
        free(fs);
        return NULL;
    }

    fs->fat_start_sector = fs->boot.reserved_sectors;
    uint32_t fat_size = fs->boot.sectors_per_fat_32;
    uint32_t root_dir_sectors = 0;

    fs->data_start_sector = fs->boot.reserved_sectors +
                            (fs->boot.num_fats * fat_size) + root_dir_sectors;

    uint32_t total_sectors = fs->boot.total_sectors_32;
    uint32_t data_sectors = total_sectors - fs->data_start_sector;

    fs->total_clusters = data_sectors / fs->boot.sectors_per_cluster;
    fs->bytes_per_cluster =
        fs->boot.sectors_per_cluster * fs->boot.bytes_per_sector;

    printkf_ok("FAT32 filesystem mounted:\n");
    printkf_info("  Volume label: %.11s\n", fs->boot.volume_label);
    printkf_info("  Bytes per sector: %u\n", fs->boot.bytes_per_sector);
    printkf_info("  Sectors per cluster: %u\n", fs->boot.sectors_per_cluster);
    printkf_info("  Bytes per cluster: %u\n", fs->bytes_per_cluster);
    printkf_info("  Reserved sectors: %u\n", fs->boot.reserved_sectors);
    printkf_info("  Number of FATs: %u\n", fs->boot.num_fats);
    printkf_info("  Sectors per FAT: %u\n", fs->boot.sectors_per_fat_32);
    printkf_info("  Root cluster: %u\n", fs->boot.root_cluster);
    printkf_info("  Total clusters: %u\n", fs->total_clusters);

    size_t fat_size_bytes = fat_size * fs->boot.bytes_per_sector;
    fs->fat_cache = (uint32_t*)malloc(fat_size_bytes);
    if (!fs->fat_cache) {
        printkf_error("fat32_mount(): Failed to allocate FAT cache\n");
        vfs_close(fd);
        free(fs);
        return NULL;
    }

    vfs_seek(fd, fs->fat_start_sector * fs->boot.bytes_per_sector, SEEK_SET);
    if (vfs_read(fd, fs->fat_cache, fat_size_bytes) !=
        (int64_t)fat_size_bytes) {
        printkf_error("fat32_mount(): Failed to read FAT\n");
        free(fs->fat_cache);
        vfs_close(fd);
        free(fs);
        return NULL;
    }

    fs->fat_cache_dirty = false;

    printkf_ok("FAT loaded into memory (%u KB)\n",
               (uint32_t)(fat_size_bytes / 1024));

    return fs;
}

void fat32_unmount(fat32_fs_t* fs) {
    if (!fs) return;

    fat32_flush_fat(fs);

    if (fs->fat_cache) {
        free(fs->fat_cache);
    }

    if (fs->device_fd >= 0) {
        vfs_close(fs->device_fd);
    }

    free(fs);
}

void fat32_list_root(fat32_fs_t* fs) {
    if (!fs) return;

    printkf_info("Listing root directory:\n");

    fat32_dir_entry_t* entries;
    int count;

    if (fat32_read_directory(fs, fs->boot.root_cluster, &entries, &count) < 0) {
        printkf_error("fat32_list_root(): Failed to read root directory\n");
        return;
    }

    printkf_info("Found %d entries:\n", count);

    for (int i = 0; i < count; i++) {
        fat32_dir_entry_t* entry = &entries[i];

        char filename[13];
        fat32_format_filename(entry->name, filename);

        uint32_t cluster =
            ((uint32_t)entry->cluster_high << 16) | entry->cluster_low;

        char type = (entry->attributes & FAT32_ATTR_DIRECTORY) ? 'D' : 'F';

        printkf_info("  [%c] %-12s  %8u bytes  cluster %u\n", type, filename,
                     entry->file_size, cluster);
    }

    free(entries);
}

typedef struct {
    fat32_fs_t* fs;
    fat32_dir_entry_t entry;
} fat32_node_data_t;

static int64_t fat32_vfs_read(vfs_node_t* node, void* buf, size_t size,
                              size_t offset) {
    if (!node->data) return -1;

    fat32_node_data_t* node_data = (fat32_node_data_t*)node->data;

    if (offset >= node_data->entry.file_size) {
        return 0;
    }

    if (offset + size > node_data->entry.file_size) {
        size = node_data->entry.file_size - offset;
    }

    uint8_t* file_buffer = (uint8_t*)malloc(node_data->entry.file_size);
    if (!file_buffer) return -1;

    int bytes_read = fat32_read_file(node_data->fs, &node_data->entry,
                                     file_buffer, node_data->entry.file_size);
    if (bytes_read < 0) {
        free(file_buffer);
        return -1;
    }

    memcpy(buf, file_buffer + offset, size);
    free(file_buffer);

    return size;
}

static int64_t fat32_vfs_write(vfs_node_t* node, const void* buf, size_t size,
                               size_t offset) {
    if (!node->data) return -1;

    fat32_node_data_t* node_data = (fat32_node_data_t*)node->data;

    bool append = (offset == node_data->entry.file_size);

    int bytes_written =
        fat32_write_file(node_data->fs, &node_data->entry, buf, size, append);
    if (bytes_written < 0) {
        return -1;
    }

    node->size = node_data->entry.file_size;

    fat32_flush_fat(node_data->fs);

    return bytes_written;
}

static vfs_node_t* fat32_vfs_create(vfs_node_t* parent, const char* name,
                                    vfs_node_type_t type) {
    if (!parent->data) return NULL;

    fat32_node_data_t* parent_data = (fat32_node_data_t*)parent->data;
    fat32_fs_t* fs = parent_data->fs;

    uint32_t parent_cluster =
        ((uint32_t)parent_data->entry.cluster_high << 16) |
        parent_data->entry.cluster_low;

    if (type == VFS_FILE) {
        if (fat32_create_file(fs, parent_cluster, name) < 0) {
            return NULL;
        }
    } else if (type == VFS_DIRECTORY) {
        if (fat32_create_directory(fs, parent_cluster, name) < 0) {
            return NULL;
        }
    } else {
        return NULL;
    }

    fat32_dir_entry_t* new_entry = fat32_find_file(fs, parent_cluster, name);
    if (!new_entry) {
        return NULL;
    }

    vfs_node_t* node = (vfs_node_t*)malloc(sizeof(vfs_node_t));
    memset(node, 0, sizeof(vfs_node_t));

    strncpy(node->name, name, VFS_MAX_NAME - 1);
    node->type = type;
    node->size = new_entry->file_size;
    node->ops = parent->ops;

    fat32_node_data_t* node_data =
        (fat32_node_data_t*)malloc(sizeof(fat32_node_data_t));
    node_data->fs = fs;
    memcpy(&node_data->entry, new_entry, sizeof(fat32_dir_entry_t));
    node->data = node_data;

    node->parent = parent;
    node->next = parent->children;
    parent->children = node;

    free(new_entry);

    return node;
}

static vfs_ops_t fat32_vfs_ops = {
    .read = fat32_vfs_read,
    .write = fat32_vfs_write,
    .create = fat32_vfs_create,
    .unlink = NULL,
    .truncate = NULL,
};

static void fat32_populate_vfs_dir(fat32_fs_t* fs, vfs_node_t* vfs_dir,
                                   uint32_t cluster) {
    fat32_dir_entry_t* entries;
    int count;

    if (fat32_read_directory(fs, cluster, &entries, &count) < 0) {
        return;
    }

    for (int i = 0; i < count; i++) {
        fat32_dir_entry_t* entry = &entries[i];

        char filename[13];
        fat32_format_filename(entry->name, filename);

        vfs_node_t* node = (vfs_node_t*)malloc(sizeof(vfs_node_t));
        memset(node, 0, sizeof(vfs_node_t));

        strncpy(node->name, filename, VFS_MAX_NAME - 1);
        node->type = (entry->attributes & FAT32_ATTR_DIRECTORY) ? VFS_DIRECTORY
                                                                : VFS_FILE;
        node->size = entry->file_size;
        node->ops = &fat32_vfs_ops;

        fat32_node_data_t* node_data =
            (fat32_node_data_t*)malloc(sizeof(fat32_node_data_t));
        node_data->fs = fs;
        memcpy(&node_data->entry, entry, sizeof(fat32_dir_entry_t));
        node->data = node_data;

        node->parent = vfs_dir;
        node->next = vfs_dir->children;
        vfs_dir->children = node;

        if (node->type == VFS_DIRECTORY) {
            uint32_t subdir_cluster =
                ((uint32_t)entry->cluster_high << 16) | entry->cluster_low;
            if (subdir_cluster >= 2) {
                fat32_populate_vfs_dir(fs, node, subdir_cluster);
            }
        }
    }

    free(entries);
}

int fat32_mount_vfs(const char* device_path, const char* mountpoint,
                    void** fs_data_out) {
    fat32_fs_t* fs = fat32_mount(device_path);
    if (!fs) {
        return -1;
    }

    vfs_node_t* mount_node = vfs_lookup(mountpoint);
    if (!mount_node) {
        mount_node = vfs_create(mountpoint, VFS_DIRECTORY);
        if (!mount_node) {
            printkf_error("fat32_mount_vfs(): Failed to create mountpoint\n");
            fat32_unmount(fs);
            return -1;
        }
    }

    fat32_populate_vfs_dir(fs, mount_node, fs->boot.root_cluster);

    *fs_data_out = fs;

    return 0;
}

void fat32_unmount_vfs(void* fs_data, const char* mountpoint) {
    if (!fs_data) return;

    fat32_fs_t* fs = (fat32_fs_t*)fs_data;

    // cleanup vfs
    (void)mountpoint;

    fat32_unmount(fs);
}