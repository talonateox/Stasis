#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct vfs_node vfs_node_t;
typedef struct vfs_ops vfs_ops_t;

typedef enum {
    VFS_FILE,
    VFS_DIRECTORY,
} vfs_node_type_t;

#define O_RDONLY    0x0000
#define O_WRONLY    0x0001
#define O_RDWR      0x0002
#define O_CREAT     0x0040
#define O_TRUNC     0x0200
#define O_APPEND    0x0400

#define SEEK_SET    0
#define SEEK_CUR    1
#define SEEK_END    2

#define VFS_MAX_NAME    256
#define VFS_MAX_PATH    4096

struct vfs_node {
    char name[VFS_MAX_NAME];
    vfs_node_type_t type;
    uint64_t size;

    vfs_node_t* parent;
    vfs_node_t* children;
    vfs_node_t* next;

    void* data;

    vfs_ops_t* ops;
};

struct vfs_ops {
    int64_t (*read)(vfs_node_t* node, void* buf, size_t size, size_t offset);
    int64_t (*write)(vfs_node_t* node, const void* buf, size_t size, size_t offset);
    vfs_node_t* (*create)(vfs_node_t* parent, const char* name, vfs_node_type_t type);
    int (*unlink)(vfs_node_t* node);
    int (*truncate)(vfs_node_t* node, size_t size);
};

typedef struct {
    vfs_node_t* node;
    int flags;
    size_t offset;
    bool in_use;
} file_descriptor_t;

void vfs_init();

vfs_node_t* vfs_lookup(const char* path);
vfs_node_t* vfs_create(const char* path, vfs_node_type_t type);
int vfs_unlink(const char* path);

int vfs_open(const char* path, int flags);
int vfs_close(int fd);
int64_t vfs_read(int fd, void* buf, size_t size);
int64_t vfs_write(int fd, const void* buf, size_t size);
int64_t vfs_seek(int fd, int64_t offset, int whence);
int64_t vfs_tell(int fd);

int vfs_mkdir(const char* path);
int vfs_readdir(int fd, char* name, size_t name_size);

vfs_node_t* vfs_root();
