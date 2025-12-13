#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct device device_t;
typedef struct driver driver_t;
typedef struct bus_type bus_type_t;
typedef struct device_ops device_ops_t;

typedef enum {
    DEVICE_TYPE_UNKNOWN,
    DEVICE_TYPE_PCI,
    DEVICE_TYPE_USB,
    DEVICE_TYPE_BLOCK,
    DEVICE_TYPE_CHAR,
    DEVICE_TYPE_NET
} device_type_t;

struct device {
    char name[64];
    device_type_t type;

    bus_type_t *bus;
    driver_t *driver;

    device_t *parent;
    device_t *children;
    device_t *next;

    void *driver_data;
    void *bus_data;

    device_ops_t *ops;
};

struct device_ops {
    int (*probe)(device_t *dev);
    int (*remove)(device_t *dev);
    int (*suspend)(device_t *dev);
    int (*resume)(device_t *dev);
};

struct driver {
    const char *name;
    bus_type_t *bus;

    int (*probe)(device_t *dev);
    int (*remove)(device_t *dev);

    driver_t *next;
};

struct bus_type {
    const char *name;

    int (*match)(device_t *dev, driver_t *drv);
    int (*probe)(device_t *dev);
    int (*remove)(device_t *dev);

    device_t *devices;
    driver_t *drivers;
};

void driver_manager_init();
int device_register(device_t *dev);
int device_unregister(device_t *dev);
int driver_register(driver_t *drv);
int driver_unregister(driver_t *drv);
int bus_register(bus_type_t *bus);

device_t *device_create(const char *name, device_type_t type);
void device_set_driver_data(device_t *dev, void *data);
void *device_get_driver_data(device_t *dev);