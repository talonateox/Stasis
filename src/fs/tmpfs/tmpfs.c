#include "tmpfs.h"

#include "../../io/terminal.h"
#include "../../mem/alloc/heap.h"
#include "../../std/string.h"

typedef struct {
    void* data;
    size_t capacity;
} tmpfs_file_t;

static int64_t tmpfs_read(vfs_node_t* node, void* buf, size_t size,
                          size_t offset);
static int64_t tmpfs_write(vfs_node_t* node, const void* buf, size_t size,
                           size_t offset);
static vfs_node_t* tmpfs_create(vfs_node_t* parent, const char* name,
                                vfs_node_type_t type);
static int tmpfs_delete(vfs_node_t* node);
static int tmpfs_truncate(vfs_node_t* node, size_t size);

static vfs_ops_t tmpfs_ops = {
    .read = tmpfs_read,
    .write = tmpfs_write,
    .create = tmpfs_create,
    .unlink = tmpfs_delete,
    .truncate = tmpfs_truncate,
};

void tmpfs_init() {
    vfs_node_t* root = vfs_root();
    root->ops = &tmpfs_ops;
    printkf_ok("tmpfs mounted at /\n");
}

static int64_t tmpfs_read(vfs_node_t* node, void* buf, size_t size,
                          size_t offset) {
    if (node->type != VFS_FILE) return -1;

    tmpfs_file_t* file = (tmpfs_file_t*)node->data;
    if (file == NULL || file->data == NULL) return 0;

    if (offset >= node->size) return 0;
    if (offset + size > node->size) {
        size = node->size - offset;
    }

    memcpy(buf, (uint8_t*)file->data + offset, size);
    return size;
}

static int64_t tmpfs_write(vfs_node_t* node, const void* buf, size_t size,
                           size_t offset) {
    if (node->type != VFS_FILE) return -1;

    tmpfs_file_t* file = (tmpfs_file_t*)node->data;

    if (file == NULL) {
        file = (tmpfs_file_t*)malloc(sizeof(tmpfs_file_t));
        file->data = NULL;
        file->capacity = 0;
        node->data = file;
    }

    size_t required = offset + size;

    if (required > file->capacity) {
        size_t new_capacity = (required + 4095) & ~4095;
        void* new_data = malloc(new_capacity);

        if (new_data == NULL) return -1;

        if (file->data != NULL) {
            memcpy(new_data, file->data, node->size);
            free(file->data);
        }

        if (new_capacity > node->size) {
            memset((uint8_t*)new_data + node->size, 0,
                   new_capacity - node->size);
        }

        file->data = new_data;
        file->capacity = new_capacity;
    }

    memcpy((uint8_t*)file->data + offset, buf, size);

    if (offset + size > node->size) {
        node->size = offset + size;
    }

    return size;
}

static vfs_node_t* tmpfs_create(vfs_node_t* parent, const char* name,
                                vfs_node_type_t type) {
    vfs_node_t* node = (vfs_node_t*)malloc(sizeof(vfs_node_t));
    if (node == NULL) return NULL;

    memset(node, 0, sizeof(vfs_node_t));
    strcpy(node->name, name);
    node->type = type;
    node->ops = &tmpfs_ops;
    node->parent = parent;

    node->next = parent->children;
    parent->children = node;

    return node;
}

static int tmpfs_delete(vfs_node_t* node) {
    if (node->type == VFS_FILE && node->data != NULL) {
        tmpfs_file_t* file = (tmpfs_file_t*)node->data;
        if (file->data != NULL) {
            free(file->data);
        }
        free(file);
    }
    return 0;
}

static int tmpfs_truncate(vfs_node_t* node, size_t size) {
    if (node->type != VFS_FILE) return -1;

    tmpfs_file_t* file = (tmpfs_file_t*)node->data;

    if (size == 0) {
        if (file != NULL) {
            if (file->data != NULL) {
                free(file->data);
                file->data = NULL;
            }
            file->capacity = 0;
        }
        node->size = 0;
    } else if (size < node->size) {
        node->size = size;
    } else if (size > node->size) {
        if (file == NULL) {
            file = (tmpfs_file_t*)malloc(sizeof(tmpfs_file_t));
            file->data = NULL;
            file->capacity = 0;
            node->data = file;
        }

        if (size > file->capacity) {
            size_t new_capacity = (size + 4095) & ~4095;
            void* new_data = malloc(new_capacity);
            if (new_data == NULL) return -1;

            if (file->data != NULL) {
                memcpy(new_data, file->data, node->size);
                free(file->data);
            }

            memset((uint8_t*)new_data + node->size, 0,
                   new_capacity - node->size);
            file->data = new_data;
            file->capacity = new_capacity;
        }

        node->size = size;
    }

    return 0;
}
