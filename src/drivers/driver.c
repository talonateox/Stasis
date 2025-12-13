#include "driver.h"

#include "../io/terminal.h"
#include "../mem/alloc/heap.h"
#include "../std/string.h"

static bus_type_t *bus_list = NULL;

void driver_manager_init(void) {
    bus_list = NULL;
    printkf_ok("Driver Manager Initialized\n");
}

int device_register(device_t *dev) {
    if (!dev || !dev->bus)
        return -1;

    bus_type_t *bus = dev->bus;

    dev->next = bus->devices;
    bus->devices = dev;

    driver_t *drv = bus->drivers;
    while (drv) {
        if (bus->match && bus->match(dev, drv) == 0) {
            dev->driver = drv;
            if (drv->probe) {
                int ret = drv->probe(dev);
                if (ret == 0) {
                    printkf_ok("Driver %s claimed device %s\n", drv->name, dev->name);
                    return 0;
                }
            }
        }
        drv = drv->next;
    }
    return 0;
}

int device_unregister(device_t *dev) {
    if (!dev || !dev->bus)
        return -1;

    if (dev->driver && dev->driver->remove) {
        dev->driver->remove(dev);
    }

    bus_type_t *bus = dev->bus;
    device_t **curr = &bus->devices;
    while (*curr) {
        if (*curr == dev) {
            *curr = dev->next;
            break;
        }
        curr = &(*curr)->next;
    }

    free(dev);
    return 0;
}

int driver_register(driver_t *drv) {
    if (!drv || !drv->bus)
        return -1;

    bus_type_t *bus = drv->bus;

    drv->next = bus->drivers;
    bus->drivers = drv;

    printkf_info("Registered driver %s on bus %s\n", drv->name, bus->name);

    device_t *dev = bus->devices;
    while (dev) {
        if (!dev->driver && bus->match && bus->match(dev, drv) == 0) {
            dev->driver = drv;

            if (drv->probe) {
                int ret = drv->probe(dev);
                if (ret == 0) {
                    printkf_ok("Driver %s claimed device %s\n", drv->name, dev->name);
                }
            }
        }
        dev = dev->next;
    }

    return 0;
}

int driver_unregister(driver_t *drv) {
    if (!drv || !drv->bus)
        return -1;

    device_t *dev = drv->bus->devices;
    while (dev) {
        if (dev->driver == drv) {
            if (drv->remove)
                drv->remove(dev);
            dev->driver = NULL;
        }
        dev = dev->next;
    }

    driver_t **curr = &drv->bus->drivers;
    while (*curr) {
        if (*curr == drv) {
            *curr = drv->next;
            break;
        }
        curr = &(*curr)->next;
    }

    return 0;
}

int bus_register(bus_type_t *bus) {
    if (!bus)
        return -1;

    bus->devices = NULL;
    bus->drivers = NULL;
    bus_list = bus;

    printkf_info("Registered bus: %s\n", bus->name);
    return 0;
}

device_t *device_create(const char *name, device_type_t type) {
    device_t *dev = (device_t *)malloc(sizeof(device_t));
    if (!dev)
        return NULL;

    memset(dev, 0, sizeof(device_t));
    strncpy(dev->name, name, sizeof(dev->name) - 1);
    dev->type = type;

    return dev;
}

void device_set_driver_data(device_t *dev, void *data) {
    if (dev)
        dev->driver_data = data;
}

void *device_get_driver_data(device_t *dev) {
    return dev ? dev->driver_data : NULL;
}
