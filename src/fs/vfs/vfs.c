#include "vfs.h"

#include "../../mem/alloc/heap.h"
#include "../../std/string.h"
#include "../../io/terminal.h"

static vfs_node_t* root_node = NULL;

#define MAX_FDS 256
static file_descriptor_t fd_table[MAX_FDS];

void vfs_init() {
    printkf_info("Initializing VFS...\n");
    for(int i = 0; i < MAX_FDS; i++) {
        fd_table[i].in_use = false;
    }

    root_node = (vfs_node_t*)malloc(sizeof(vfs_node_t));
    memset(root_node, 0, sizeof(vfs_node_t));

    root_node->name[0] = '/';
    root_node->name[1] = '\0';
    root_node->type = VFS_DIRECTORY;
    root_node->parent = root_node;

    printkf_ok("VFS Initialized\n");
}

vfs_node_t* vfs_root() {
    return root_node;
}

static int alloc_fd() {
    for(int i = 3; i < MAX_FDS; i++) {
        if(!fd_table[i].in_use) {
            fd_table[i].in_use = true;
            return i;
        }
    }

    return -1;
}

static file_descriptor_t* get_fd(int fd) {
    if(fd < 0 || fd >= MAX_FDS) return NULL;
    if(!fd_table[fd].in_use) return NULL;
    return &fd_table[fd];
}

static vfs_node_t* find_child(vfs_node_t* dir, const char* name) {
    if(dir->type != VFS_DIRECTORY) return NULL;

    vfs_node_t* child = dir->children;
    while(child != NULL) {
        if(strcmp(child->name, name) == 0) {
            return child;
        }
        child = child->next;
    }
    return NULL;
}

static void add_child(vfs_node_t* dir, vfs_node_t* child) {
    child->parent = dir;
    child->next = dir->children;
    dir->children = child;
}

static void remove_child(vfs_node_t* dir, vfs_node_t* child) {
    if (dir->children == child) {
        dir->children = child->next;
        return;
    }

    vfs_node_t* prev = dir->children;
    while (prev != NULL && prev->next != child) {
        prev = prev->next;
    }
    if (prev != NULL) {
        prev->next = child->next;
    }
}

vfs_node_t* vfs_lookup(const char* path) {
    if (path == NULL || path[0] == '\0') return NULL;
    if (path[0] != '/') return NULL;
    if (path[0] == '/' && path[1] == '\0') return root_node;

    vfs_node_t* current = root_node;
    char component[VFS_MAX_NAME];
    int comp_idx = 0;

    for (int i = 1; path[i - 1] != '\0'; i++) {
        if (path[i] == '/' || path[i] == '\0') {
            if (comp_idx > 0) {
                component[comp_idx] = '\0';

                if (strcmp(component, ".") == 0) {
                } else if (strcmp(component, "..") == 0) {
                    current = current->parent;
                } else {
                    current = find_child(current, component);
                    if (current == NULL) {
                        return NULL;
                    }
                }
                comp_idx = 0;
            }
        } else {
            if (comp_idx < VFS_MAX_NAME - 1) {
                component[comp_idx++] = path[i];
            }
        }
    }

    return current;
}

vfs_node_t* vfs_create(const char* path, vfs_node_type_t type) {
    if (path == NULL || path[0] != '/') return NULL;

    char parent_path[VFS_MAX_PATH];
    char name[VFS_MAX_NAME];

    int last_slash = -1;
    for (int i = 0; path[i] != '\0'; i++) {
        if (path[i] == '/') last_slash = i;
    }

    if (last_slash <= 0) {
        parent_path[0] = '/';
        parent_path[1] = '\0';
        strcpy(name, path + 1);
    } else {
        memcpy(parent_path, path, last_slash);
        parent_path[last_slash] = '\0';
        strcpy(name, path + last_slash + 1);
    }

    if (name[0] == '\0') return NULL;

    vfs_node_t* parent = vfs_lookup(parent_path);
    if (parent == NULL || parent->type != VFS_DIRECTORY) {
        return NULL;
    }

    if (find_child(parent, name) != NULL) {
        return NULL;
    }

    if (parent->ops && parent->ops->create) {
        return parent->ops->create(parent, name, type);
    }

    vfs_node_t* node = (vfs_node_t*)malloc(sizeof(vfs_node_t));
    memset(node, 0, sizeof(vfs_node_t));
    strcpy(node->name, name);
    node->type = type;
    node->ops = parent->ops;

    add_child(parent, node);

    return node;
}

int vfs_unlink(const char* path, bool recursive) {
    vfs_node_t* node = vfs_lookup(path);
    if (node == NULL) return -1;
    if (node == root_node) return -1;

    if(node->type == VFS_DIRECTORY) {
        if(node->children != NULL) {
            if(!recursive) return -1;

            vfs_node_t* child = node->children;
            if(child != NULL) {
                vfs_node_t* next = child->next;

                char child_path[VFS_MAX_PATH];
                size_t path_len = strlen(path);
                size_t name_len = strlen(child->name);

                if(path_len + 1 + name_len + 1 > VFS_MAX_PATH) {
                    return -1;
                }

                memcpy(child_path, path, path_len);
                if(path_len > 0 && path[path_len - 1] != '/') {
                    child_path[path_len] = '/';
                    path_len++;
                }
                memcpy(child_path + path_len, child->name, name_len);
                child_path[path_len + name_len] = '\0';

                int result = vfs_unlink(child_path, true);
                if(result < 0) {
                    return result;
                }
                child = next;
            }
        }
    }

    if(node->ops && node->ops->unlink) {
        int res = node->ops->unlink(node);
        if(res < 0) return res;
    }

    remove_child(node->parent, node);

    free(node);

    return 0;
}

int vfs_open(const char* path, int flags) {
    vfs_node_t* node = vfs_lookup(path);

    if (node == NULL && (flags & O_CREAT)) {
        node = vfs_create(path, VFS_FILE);
    }

    if (node == NULL) return -1;
    if (node->type == VFS_DIRECTORY && (flags & (O_WRONLY | O_RDWR))) {
        return -1;
    }

    if ((flags & O_TRUNC) && node->ops && node->ops->truncate) {
        node->ops->truncate(node, 0);
    }

    int fd = alloc_fd();
    if (fd < 0) return -1;

    fd_table[fd].node = node;
    fd_table[fd].flags = flags;
    fd_table[fd].offset = (flags & O_APPEND) ? node->size : 0;

    return fd;
}

int vfs_close(int fd) {
    file_descriptor_t* f = get_fd(fd);
    if (f == NULL) return -1;

    f->in_use = false;
    f->node = NULL;

    return 0;
}

int64_t vfs_read(int fd, void* buf, size_t size) {
    file_descriptor_t* f = get_fd(fd);
    if (f == NULL) return -1;
    if (f->node->type == VFS_DIRECTORY) return -1;
    if ((f->flags & O_WRONLY)) return -1;

    if (f->node->ops && f->node->ops->read) {
        int64_t bytes = f->node->ops->read(f->node, buf, size, f->offset);
        if (bytes > 0) {
            f->offset += bytes;
        }
        return bytes;
    }

    return -1;
}

int64_t vfs_write(int fd, const void* buf, size_t size) {
    file_descriptor_t* f = get_fd(fd);
    if (f == NULL) return -1;
    if (f->node->type == VFS_DIRECTORY) return -1;
    if ((f->flags & O_RDONLY) && !(f->flags & O_RDWR)) return -1;

    if (f->flags & O_APPEND) {
        f->offset = f->node->size;
    }

    if (f->node->ops && f->node->ops->write) {
        int64_t bytes = f->node->ops->write(f->node, buf, size, f->offset);
        if (bytes > 0) {
            f->offset += bytes;
        }
        return bytes;
    }

    return -1;
}

int64_t vfs_seek(int fd, int64_t offset, int whence) {
    file_descriptor_t* f = get_fd(fd);
    if (f == NULL) return -1;

    int64_t new_offset;
    switch (whence) {
        case SEEK_SET:
            new_offset = offset;
            break;
        case SEEK_CUR:
            new_offset = f->offset + offset;
            break;
        case SEEK_END:
            new_offset = f->node->size + offset;
            break;
        default:
            return -1;
    }

    if (new_offset < 0) return -1;

    f->offset = new_offset;
    return new_offset;
}

int64_t vfs_tell(int fd) {
    file_descriptor_t* f = get_fd(fd);
    if (f == NULL) return -1;
    return f->offset;
}

int vfs_mkdir(const char* path) {
    vfs_node_t* node = vfs_create(path, VFS_DIRECTORY);
    return node ? 0 : -1;
}

int vfs_readdir(int fd, char* name, size_t name_size) {
    file_descriptor_t* f = get_fd(fd);
    if (f == NULL) return -1;
    if (f->node->type != VFS_DIRECTORY) return -1;

    vfs_node_t* child = f->node->children;
    size_t idx = 0;

    while (child != NULL && idx < f->offset) {
        child = child->next;
        idx++;
    }

    if (child == NULL) {
        return 0;
    }

    size_t len = strlen(child->name);
    if (len >= name_size) len = name_size - 1;
    memcpy(name, child->name, len);
    name[len] = '\0';

    f->offset++;

    return 1;
}
